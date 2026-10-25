"""ROS2 bridge between the MecanumB STM32 firmware and a MAVLink v2 companion link.

The node owns the serial port, keeps the firmware's MAVLink heartbeat and
parameter handshake alive, converts the vehicle's NED/FRD telemetry into
REP-103 ENU/FLU messages, and turns ``cmd_vel`` into BODY_NED velocity
setpoints.  The ESP32 remote-control path is not driven from here: the
firmware arbitrates between the Pi and the ESP32, and this node reports the
arbitration state and the ESP32 link health in diagnostics.

Wire it up with ``ros2 launch mecanum_mavlink bridge.launch.py`` and arm with
``ros2 service call /mecanum_mavlink/arm std_srvs/srv/SetBool "{data: true}"``.
"""

import math
import threading
import time
import traceback
from queue import Empty, Queue

import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from geometry_msgs.msg import Quaternion, TransformStamped, Twist
from nav_msgs.msg import Odometry
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import BatteryState, Imu, Joy, NavSatFix, NavSatStatus
from std_msgs.msg import Bool, String
from std_srvs.srv import SetBool, Trigger
import tf2_ros

try:  # pyserial is a runtime dependency of the bridge only.
    import serial
except ImportError:  # pragma: no cover - reported at startup
    serial = None

from pymavlink.dialects.v20 import common as mavlink

from .conversions import (
    body_frd_rates_to_flu,
    body_frd_to_flu,
    clamp,
    enu_twist_to_body_ned,
    euler_ned_to_enu,
    euler_to_quaternion,
    mavlink_battery_to_ros,
    ned_position_to_enu,
)
from .pi_gpio import ReadyLine

MAV_CMD_COMPONENT_ARM_DISARM = 400
MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN = 246
SETPOINT_TYPE_MASK = (
    mavlink.POSITION_TARGET_TYPEMASK_X_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_Y_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_Z_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_AX_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_AY_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_AZ_IGNORE
    | mavlink.POSITION_TARGET_TYPEMASK_YAW_IGNORE
)

STATE_NAMES = {
    1: "DISARMED",
    2: "ARMING",
    4: "WIFI_ACTIVE",
    5: "PI_ACTIVE",
    6: "FAILSAFE",
    7: "FAULT",
}
SOURCE_NAMES = {0: "pi", 1: "wifi"}
# Firmware fault bits, in the order the firmware documents them.
FAULT_BITS = (
    (0x01, "hardware"),
    (0x02, "control"),
    (0x04, "rx"),
    (0x08, "overcurrent"),
    (0x10, "stall"),
    (0x20, "overtemp"),
    (0x40, "undervolt"),
    (0x80, "sensor"),
)
# Electrical faults that stop motion, as opposed to the RX/timeout bit a
# healthy link clears on its own.
ELECTRICAL_FAULTS = 0x08 | 0x10 | 0x20 | 0x40 | 0x80
LOW_STACK_WORDS = 32
RESULT_NAMES = {
    0: "ACCEPTED",
    1: "TEMPORARILY_REJECTED",
    2: "DENIED",
    3: "UNSUPPORTED",
    4: "FAILED",
    5: "IN_PROGRESS",
    6: "CANCELLED",
}

# (firmware PARAM name, ROS parameter suffix, default, is_integer)
# Geometry and counts default to zero on purpose: the firmware rejects an
# invalid calibration, so the vehicle cannot arm until measured values are
# supplied in config/vehicle.yaml.
CALIBRATION_TABLE = (
    ("RADIUS_M", "radius_m", 0.0, False),
    ("HALF_LEN_M", "half_length_m", 0.0, False),
    ("HALF_WID_M", "half_width_m", 0.0, False),
    ("COUNTS_REV", "counts_per_rev", 0, True),
    ("ENC_SIGN0", "encoder_sign_0", 1, True),
    ("ENC_SIGN1", "encoder_sign_1", 1, True),
    ("ENC_SIGN2", "encoder_sign_2", 1, True),
    ("ENC_SIGN3", "encoder_sign_3", 1, True),
    ("MOT_SIGN0", "motor_sign_0", 1, True),
    ("MOT_SIGN1", "motor_sign_1", 1, True),
    ("MOT_SIGN2", "motor_sign_2", 1, True),
    ("MOT_SIGN3", "motor_sign_3", 1, True),
    ("MAX_WHEEL", "max_wheel_rad_s", 20.0, False),
    ("MAX_ACCEL", "max_wheel_accel_rad_s2", 100.0, False),
    ("KP", "wheel_kp", 0.1, False),
    ("KI", "wheel_ki", 0.2, False),
    ("FF", "feedforward", 0.02, False),
    ("DUTY_LIMIT", "duty_limit", 0.5, False),
    ("SPEED_TAU", "speed_filter_s", 0.01, False),
    ("REVERSAL", "reversal_rad_s", 0.5, False),
)


class _SerialSink:
    """File-like adapter so pymavlink can write straight to the serial port."""

    def __init__(self):
        self.ser = None
        self.lock = threading.Lock()

    def write(self, data):
        with self.lock:
            if self.ser is None:
                raise OSError("serial port is not open")
            self.ser.write(data)
        return len(data)


class MavlinkBridge(Node):
    """MAVLink v2 <-> ROS2 bridge for a MecanumB ground rover."""

    def __init__(self, **kwargs):
        super().__init__("mecanum_mavlink", **kwargs)

        self.port = self.declare_parameter("port", "/dev/ttyAMA0").value
        self.baud = int(self.declare_parameter("baud", 921600).value)
        self.target_system = int(self.declare_parameter("target_system", 1).value)
        self.target_component = int(self.declare_parameter("target_component", 1).value)
        self.source_system = int(self.declare_parameter("source_system", 255).value)
        self.source_component = int(self.declare_parameter("source_component", 191).value)
        self.setpoint_rate = float(self.declare_parameter("setpoint_rate_hz", 20.0).value)
        self.imu_rate = float(self.declare_parameter("imu_rate_hz", 50.0).value)
        self.odom_rate = float(self.declare_parameter("odom_rate_hz", 20.0).value)
        self.command_timeout = float(self.declare_parameter("command_timeout_s", 0.5).value)
        self.max_speed = float(self.declare_parameter("max_speed_mps", 1.0).value)
        self.max_yaw_rate = float(self.declare_parameter("max_yaw_rate_rad_s", 2.0).value)
        self.heartbeat_timeout = float(self.declare_parameter("heartbeat_timeout_s", 2.0).value)
        self.autoconfigure = bool(self.declare_parameter("autoconfigure", True).value)
        self.publish_tf = bool(self.declare_parameter("publish_tf", True).value)
        self.ready_gpiochip = self.declare_parameter("ready_gpiochip", "").value
        self.ack_timeout = float(self.declare_parameter("command_ack_timeout_s", 1.0).value)
        self.odom_frame = self.declare_parameter("odom_frame", "odom").value
        self.base_frame = self.declare_parameter("base_frame", "base_link").value
        self.imu_frame = self.declare_parameter("imu_frame", "imu_link").value

        self._calibration = {}
        for name, suffix, default, is_int in CALIBRATION_TABLE:
            value = self.declare_parameter("calibration." + suffix, default).value
            self._calibration[name] = float(int(value) if is_int else value)

        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._outbox = Queue()
        self._sink = _SerialSink()
        self._serial_missing_reported = False

        # MAVLink receive state.
        self.mav = mavlink.MAVLink(
            self._sink, srcSystem=self.source_system, srcComponent=self.source_component
        )
        self.mav.robust_parsing = True
        self._last_heartbeat = 0.0
        self._armed = False
        self._system_status = 0
        self._state = 0
        self._source = -1
        self._fault_bits = 0
        self._attitude = None
        self._attitude_time = 0.0
        self._imu = None
        self._imu_time = 0.0
        self._position = None
        self._position_time = 0.0
        self._odom = None
        self._odom_time = 0.0
        self._battery = None
        self._battery_time = 0.0
        self._gps = None
        self._gps_time = 0.0
        self._rc = None
        self._rc_time = 0.0
        self._named = {}
        self._statustext = []
        self._ack_lock = threading.Lock()
        self._ack_waiters = {}
        self._shutdown_pending = False
        self._ping_tx = {}
        self._ping_seq = 0
        self._rtt = float("nan")

        # READY is the hardware half of the Pi interlock: the firmware refuses
        # PI authority while it is low, and the power board uses the same line
        # to see the host go away. Claim it before any traffic is sent.
        self.ready_line = None
        if self.ready_gpiochip:
            try:
                self.ready_line = ReadyLine(self.ready_gpiochip)
            except (ImportError, OSError, RuntimeError, AttributeError) as exc:
                self.get_logger().fatal("cannot claim Pi READY GPIO17: %s" % exc)
                super().destroy_node()
                raise

        # Parameter handshake state.
        self._param_echo = {}
        self._param_next = {}
        self._param_attempts = {}
        # With autoconfigure disabled another tool owns the calibration; arm
        # attempts are still forwarded so the firmware's own verdict is seen.
        self._configured = not self.autoconfigure
        self._next_verify = 0.0

        # Command state.
        self._cmd = (0.0, 0.0, 0.0)
        self._cmd_time = 0.0

        self._boot = time.monotonic()

        sensor_qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.pub_imu = self.create_publisher(Imu, "imu/data", sensor_qos)
        self.pub_odom = self.create_publisher(Odometry, "odom", 10)
        self.pub_battery = self.create_publisher(BatteryState, "battery", 10)
        self.pub_fix = self.create_publisher(NavSatFix, "fix", 10)
        self.pub_rc = self.create_publisher(Joy, "rc/channels", 10)
        self.pub_armed = self.create_publisher(Bool, "~/armed", 10)
        self.pub_diag = self.create_publisher(DiagnosticArray, "/diagnostics", 10)
        self.pub_statustext = self.create_publisher(String, "~/statustext", 10)
        self.create_subscription(Twist, "cmd_vel", self._on_cmd_vel, 10)
        self.create_service(
            SetBool, "~/arm", self._on_arm, callback_group=ReentrantCallbackGroup()
        )
        self.create_service(
            Trigger, "~/shutdown_pi", self._on_shutdown,
            callback_group=ReentrantCallbackGroup(),
        )

        self._tf_broadcaster = tf2_ros.TransformBroadcaster(self) if self.publish_tf else None

        self._next_imu = 0.0
        self._next_odom = 0.0
        self._next_slow = 0.0
        self._next_rc = 0.0
        self._next_setpoint = 0.0
        self._next_heartbeat = 0.0
        self._next_ping = 0.0
        self._next_config = 0.0
        self._next_status = 0.0

        self._io_thread = threading.Thread(
            target=self._io_loop, name="mavlink-io", daemon=True
        )
        self._io_thread.start()
        self.create_timer(0.02, self._on_timer)

        self.get_logger().info(
            "MAVLink bridge on %s @ %d (target %d:%d, source %d:%d)"
            % (
                self.port,
                self.baud,
                self.target_system,
                self.target_component,
                self.source_system,
                self.source_component,
            )
        )

    # ------------------------------------------------------------------ serial

    def _open_port(self):
        if serial is None:
            if not self._serial_missing_reported:
                self.get_logger().error(
                    "pyserial is not installed; install python3-serial to use the bridge"
                )
                self._serial_missing_reported = True
            return False
        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.02, write_timeout=0.2)
        except (serial.SerialException, OSError) as exc:
            self.get_logger().warning("cannot open %s: %s" % (self.port, exc), throttle_duration_sec=5.0)
            return False
        with self._sink.lock:
            self._sink.ser = ser
        self.get_logger().info("serial link open on %s" % self.port)
        return True

    def _close_port(self):
        with self._sink.lock:
            ser, self._sink.ser = self._sink.ser, None
        if ser is not None:
            try:
                ser.close()
            except (serial.SerialException, OSError):
                pass

    def _send(self, fn):
        if self._outbox.qsize() > 512:  # never grow without bound while unplugged
            return
        self._outbox.put(fn)

    def _io_loop(self):
        while not self._stop.is_set() and rclpy.ok():
            # Hold the port locally: another thread may clear the sink while
            # this loop is between the check and the read.
            ser = self._sink.ser
            if ser is None:
                if not self._open_port():
                    self._stop.wait(1.0)
                    continue
                ser = self._sink.ser
            try:
                while True:
                    try:
                        fn = self._outbox.get_nowait()
                    except Empty:
                        break
                    fn()
                waiting = ser.in_waiting
                data = ser.read(waiting if waiting else 1)
                if data:
                    for msg in self.mav.parse_buffer(data) or ():
                        self._handle(msg)
            except (OSError, serial.SerialException) as exc:
                if self._stop.is_set():
                    break
                self.get_logger().error("serial link lost: %s" % exc)
                self._close_port()
                self._stop.wait(1.0)
            except Exception as exc:  # keep the bridge alive on malformed input
                self.get_logger().error(
                    "MAVLink receive error: %s\n%s" % (exc, traceback.format_exc())
                )
                self._stop.wait(0.1)

    # ------------------------------------------------------------- rx handling

    def _handle(self, msg):
        now = time.monotonic()
        if msg.get_srcSystem() != self.target_system:
            return
        if msg.get_srcComponent() != self.target_component:
            return
        kind = msg.get_type()

        if kind == "HEARTBEAT":
            with self._lock:
                self._last_heartbeat = now
                self._armed = bool(msg.base_mode & mavlink.MAV_MODE_FLAG_SAFETY_ARMED)
                self._system_status = msg.system_status
        elif kind == "COMMAND_ACK":
            # The firmware echoes our sysid as the ACK target; a 0 target from
            # other autopilots is also addressed to everyone.
            if msg.target_system in (0, self.source_system):
                with self._ack_lock:
                    waiter = self._ack_waiters.get(msg.command)
                if waiter is not None:
                    waiter[1][0] = msg.result
                    waiter[0].set()
        elif kind == "ATTITUDE":
            with self._lock:
                self._attitude = (
                    msg.roll,
                    msg.pitch,
                    msg.yaw,
                    msg.rollspeed,
                    msg.pitchspeed,
                    msg.yawspeed,
                )
                self._attitude_time = now
        elif kind == "HIGHRES_IMU":
            with self._lock:
                self._imu = (
                    msg.xacc,
                    msg.yacc,
                    msg.zacc,
                    msg.xgyro,
                    msg.ygyro,
                    msg.zgyro,
                    msg.temperature,
                )
                self._imu_time = now
        elif kind == "LOCAL_POSITION_NED":
            with self._lock:
                self._position = (msg.x, msg.y, msg.z, msg.vx, msg.vy, msg.vz)
                self._position_time = now
        elif kind == "ODOMETRY":
            if msg.child_frame_id == mavlink.MAV_FRAME_BODY_FRD:
                with self._lock:
                    self._odom = (
                        msg.q,
                        msg.vx,
                        msg.vy,
                        msg.vz,
                        msg.rollspeed,
                        msg.pitchspeed,
                        msg.yawspeed,
                    )
                    self._odom_time = now
        elif kind == "BATTERY_STATUS":
            with self._lock:
                self._battery = (msg.voltages, msg.current_battery, msg.battery_remaining)
                self._battery_time = now
        elif kind == "GPS_RAW_INT":
            with self._lock:
                self._gps = (
                    msg.fix_type,
                    msg.lat,
                    msg.lon,
                    msg.alt,
                    msg.h_acc,
                    msg.v_acc,
                    msg.satellites_visible,
                )
                self._gps_time = now
        elif kind == "RC_CHANNELS":
            with self._lock:
                self._rc = (
                    msg.chancount,
                    [
                        getattr(msg, "chan%d_raw" % (i + 1))
                        for i in range(16)
                    ],
                )
                self._rc_time = now
        elif kind == "NAMED_VALUE_FLOAT":
            with self._lock:
                self._named[msg.name] = (msg.value, now)
                if msg.name == "state":
                    self._state = int(round(msg.value))
                elif msg.name == "src":
                    self._source = int(round(msg.value))
                elif msg.name == "fault":
                    self._fault_bits = int(round(msg.value))
        elif kind == "STATUSTEXT":
            with self._lock:
                self._statustext.append((msg.severity, msg.text))
                if len(self._statustext) > 16:
                    del self._statustext[:-16]
        elif kind == "PING":
            sent = self._ping_tx.pop(msg.seq, None)
            if sent is not None:
                self._rtt = now - sent
        elif kind == "PARAM_VALUE":
            name = msg.param_id
            with self._lock:
                self._param_echo[name] = msg.param_value
            desired = self._calibration.get(name)
            if desired is not None and abs(msg.param_value - desired) <= 1e-6 * max(
                1.0, abs(desired)
            ):
                self._param_attempts.pop(name, None)
                self._param_next.pop(name, None)
                self._update_configured()

    def _update_configured(self):
        if not self.autoconfigure:
            return
        confirmed = self._calibration_valid() and all(
            name in self._param_echo
            and abs(self._param_echo[name] - value) <= 1e-6 * max(1.0, abs(value))
            for name, value in self._calibration.items()
        )
        if confirmed and not self._configured:
            self._configured = True
            self.get_logger().info("calibration accepted (%d parameters)" % len(self._calibration))
        elif not confirmed:
            self._configured = False

    def _calibration_valid(self):
        """Mirror vc_config_valid() so the service refuses impossible geometry."""
        c = self._calibration
        if not 0.01 <= c["RADIUS_M"] <= 0.5:
            return False
        if not 0.01 <= c["HALF_LEN_M"] <= 2.0 or not 0.01 <= c["HALF_WID_M"] <= 2.0:
            return False
        counts = int(c["COUNTS_REV"])
        if counts <= 0 or counts > 10_000_000:
            return False
        if not 0.0 < c["MAX_WHEEL"] <= 200.0:
            return False
        if not 0.0 < c["MAX_ACCEL"] <= 1000.0:
            return False
        if not 0.0 <= c["KP"] <= 10.0 or not 0.0 <= c["KI"] <= 100.0:
            return False
        if not 0.0 <= c["FF"] <= 10.0:
            return False
        if not 0.0 < c["DUTY_LIMIT"] <= 1.0:
            return False
        if not 0.001 <= c["SPEED_TAU"] <= 1.0 or not 0.0 <= c["REVERSAL"] <= 10.0:
            return False
        if counts * c["MAX_WHEEL"] * 0.002 / (2.0 * math.pi) >= 32768.0:
            return False
        for i in range(4):
            if c["ENC_SIGN%d" % i] not in (-1.0, 1.0):
                return False
            if c["MOT_SIGN%d" % i] not in (-1.0, 1.0):
                return False
        return True

    # -------------------------------------------------------------- tx/service

    def _on_cmd_vel(self, msg):
        vx, vy, wz = enu_twist_to_body_ned(
            msg.linear.x, msg.linear.y, msg.angular.z
        )
        self._cmd = (
            clamp(vx, self.max_speed),
            clamp(vy, self.max_speed),
            clamp(wz, self.max_yaw_rate),
        )
        self._cmd_time = time.monotonic()

    def _command(self, command, params, timeout=None):
        """Send a COMMAND_LONG and wait for its ACK, or return None.

        The waiter is registered before the frame is queued, so a fast ACK
        cannot land between the send and the wait. None means no ACK arrived
        (or a request for the same command was already in flight).
        """
        timeout = self.ack_timeout if timeout is None else timeout
        event = threading.Event()
        slot = [None]
        with self._ack_lock:
            if command in self._ack_waiters:
                return None
            self._ack_waiters[command] = (event, slot)
        try:
            self._send(
                lambda: self.mav.command_long_send(
                    self.target_system,
                    self.target_component,
                    command,
                    0,
                    params[0],
                    params[1],
                    params[2],
                    params[3],
                    params[4],
                    params[5],
                    params[6],
                )
            )
            if not event.wait(timeout=timeout):
                return None
            return slot[0]
        finally:
            with self._ack_lock:
                self._ack_waiters.pop(command, None)

    def _on_arm(self, request, response):
        arm = bool(request.data)
        if arm and not self._configured:
            response.success = False
            response.message = (
                "calibration invalid; set the measured geometry and counts in "
                "config/vehicle.yaml"
                if not self._calibration_valid()
                else "calibration not applied; refusing to arm"
            )
            return response
        params = [1.0 if arm else 0.0] + [0.0] * 6
        result = self._command(MAV_CMD_COMPONENT_ARM_DISARM, params)
        if result is None:
            response.success = False
            response.message = "no COMMAND_ACK from the flight controller"
            return response
        response.success = result == mavlink.MAV_RESULT_ACCEPTED
        response.message = "%s: %s" % (
            "arm" if arm else "disarm",
            RESULT_NAMES.get(result, "result %s" % result),
        )
        return response

    def _on_shutdown(self, request, response):
        """Ask the flight controller to power this Pi down in order.

        The firmware only accepts a companion (param2=2) shutdown while it is
        disarmed with a live Pi READY, so both are checked here first. The ACK
        means the request was latched: the power board then waits for wheel
        stop and the kernel HALTED line before cutting power. Nothing in this
        node fakes the halt.
        """
        del request
        with self._lock:
            armed = self._armed
        if armed:
            response.success = False
            response.message = "disarm before requesting Pi shutdown"
            return response
        if self._shutdown_pending:
            response.success = False
            response.message = "Pi shutdown is already pending"
            return response
        if not self._link_ok(time.monotonic()):
            response.success = False
            response.message = "no heartbeat from the flight controller"
            return response
        # Hold READY asserted until the ACK: the firmware refuses the request
        # when READY is low. A denial releases the hold on the next timer tick.
        result = self._command(
            MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN, [0.0, 2.0] + [0.0] * 5
        )
        if result != mavlink.MAV_RESULT_ACCEPTED:
            self._shutdown_pending = False
            response.success = False
            response.message = "shutdown: %s" % (
                "no COMMAND_ACK from the flight controller"
                if result is None
                else RESULT_NAMES.get(result, "result %s" % result)
            )
            return response
        self._shutdown_pending = True
        self._set_ready(False)
        response.success = True
        response.message = (
            "shutdown accepted; the power board now waits for the kernel to halt"
        )
        self.get_logger().warning(
            "Pi shutdown accepted by the flight controller; stop using this host now"
        )
        return response

    # ------------------------------------------------------------------ timers

    def _on_timer(self):
        now = time.monotonic()
        # READY follows the link: it says "this host is alive and talking", not
        # "the vehicle is armed". A pending shutdown keeps it low.
        self._set_ready(self._link_ok(now) and not self._shutdown_pending)
        if now >= self._next_heartbeat:
            self._next_heartbeat = now + 1.0
            self._send(self._heartbeat)
        if now >= self._next_ping:
            self._next_ping = now + 1.0
            self._send_ping(now)
        if now >= self._next_config:
            self._next_config = now + 0.05
            self._configure(now)
        if now >= self._next_setpoint and self._configured:
            period = 1.0 / max(1.0, self.setpoint_rate)
            self._next_setpoint = now + period
            self._send_setpoint(now)
        if now >= self._next_imu:
            self._next_imu = now + 1.0 / max(1.0, self.imu_rate)
            self._publish_imu(now)
        if now >= self._next_odom:
            self._next_odom = now + 1.0 / max(1.0, self.odom_rate)
            self._publish_odom(now)
        if now >= self._next_rc:
            self._next_rc = now + 0.1
            self._publish_rc(now)
        if now >= self._next_slow:
            self._next_slow = now + 1.0
            self._publish_battery(now)
            self._publish_fix(now)
            self._publish_statustext()
        if now >= self._next_status:
            self._next_status = now + 1.0
            self._publish_armed()
            self._publish_diagnostics(now)

    def _heartbeat(self):
        self.mav.heartbeat_send(
            mavlink.MAV_TYPE_ONBOARD_CONTROLLER,
            mavlink.MAV_AUTOPILOT_INVALID,
            0,
            0,
            mavlink.MAV_STATE_ACTIVE,
        )

    def _set_ready(self, active):
        if self.ready_line is None:
            return
        try:
            self.ready_line.set(active)
        except (RuntimeError, OSError) as exc:
            self.get_logger().error(
                "READY line update failed: %s" % exc, throttle_duration_sec=5.0
            )

    def _send_ping(self, now):
        self._ping_seq = (self._ping_seq + 1) & 0xFFFFFFFF
        seq = self._ping_seq
        self._ping_tx[seq] = now
        if len(self._ping_tx) > 8:
            self._ping_tx.pop(min(self._ping_tx))
        self._send(
            lambda: self.mav.ping_send(
                int(time.monotonic() * 1e6),
                seq,
                self.target_system,
                self.target_component,
            )
        )

    def _configure(self, now):
        if not self.autoconfigure or self._configured or not self._link_ok(now):
            return
        with self._lock:
            armed = self._armed
        if armed:
            return
        for name, value in self._calibration.items():
            if now < self._param_next.get(name, 0.0):
                continue
            if name in self._param_echo and abs(self._param_echo[name] - value) <= 1e-6 * max(
                1.0, abs(value)
            ):
                continue
            attempts = self._param_attempts.get(name, 0) + 1
            self._param_attempts[name] = attempts
            if attempts == 5:
                self.get_logger().warning(
                    "parameter %s not accepted after %d attempts (echo %s)"
                    % (name, attempts, self._param_echo.get(name))
                )
            self._param_next[name] = now + 1.0
            is_int = any(entry[0] == name and entry[3] for entry in CALIBRATION_TABLE)
            param_type = (
                mavlink.MAV_PARAM_TYPE_INT32 if is_int else mavlink.MAV_PARAM_TYPE_REAL32
            )
            self._send(
                lambda n=name, v=value, t=param_type: self.mav.param_set_send(
                    self.target_system,
                    self.target_component,
                    n.encode("ascii"),
                    v,
                    t,
                )
            )
        if now >= self._next_verify:
            self._next_verify = now + 5.0
            self._send(
                lambda: self.mav.param_request_list_send(
                    self.target_system, self.target_component
                )
            )

    def _send_setpoint(self, now):
        fresh = self._cmd_time > 0.0 and now - self._cmd_time <= self.command_timeout
        with self._lock:
            armed = self._armed
        if not armed and not fresh:
            return
        vx, vy, wz = self._cmd if fresh else (0.0, 0.0, 0.0)
        boot_ms = int((now - self._boot) * 1000.0) & 0xFFFFFFFF
        self._send(
            lambda: self.mav.set_position_target_local_ned_send(
                boot_ms,
                self.target_system,
                self.target_component,
                mavlink.MAV_FRAME_BODY_NED,
                SETPOINT_TYPE_MASK,
                0.0,
                0.0,
                0.0,
                vx,
                vy,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                wz,
            )
        )

    # -------------------------------------------------------------- publishing

    def _link_ok(self, now):
        return self._last_heartbeat > 0.0 and now - self._last_heartbeat <= self.heartbeat_timeout

    def _publish_imu(self, now):
        with self._lock:
            imu = self._imu
            imu_time = self._imu_time
            att = self._attitude
            att_time = self._attitude_time
        if imu is None or now - imu_time > 0.5 or att is None or now - att_time > 0.5:
            return
        message = Imu()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = self.imu_frame
        roll, pitch, yaw = euler_ned_to_enu(att[0], att[1], att[2])
        qw, qx, qy, qz = euler_to_quaternion(roll, pitch, yaw)
        message.orientation = Quaternion(x=qx, y=qy, z=qz, w=qw)
        message.orientation_covariance = [0.05, 0.0, 0.0, 0.0, 0.05, 0.0, 0.0, 0.0, 0.1]
        gx, gy, gz = body_frd_rates_to_flu(att[3], att[4], att[5])
        message.angular_velocity.x = gx
        message.angular_velocity.y = gy
        message.angular_velocity.z = gz
        message.angular_velocity_covariance = [
            1e-3, 0.0, 0.0, 0.0, 1e-3, 0.0, 0.0, 0.0, 1e-3
        ]
        ax, ay, az = body_frd_to_flu(imu[0], imu[1], imu[2])
        message.linear_acceleration.x = ax
        message.linear_acceleration.y = ay
        message.linear_acceleration.z = az
        message.linear_acceleration_covariance = [
            5e-2, 0.0, 0.0, 0.0, 5e-2, 0.0, 0.0, 0.0, 5e-2
        ]
        self.pub_imu.publish(message)

    def _publish_odom(self, now):
        with self._lock:
            position = self._position
            position_time = self._position_time
            odom = self._odom
            odom_time = self._odom_time
            att = self._attitude
            att_time = self._attitude_time
            imu = self._imu
            imu_time = self._imu_time
        if position is None or now - position_time > 1.0:
            return
        if att is None or now - att_time > 0.5:
            return
        message = Odometry()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = self.odom_frame
        message.child_frame_id = self.base_frame
        ex, ey, ez = ned_position_to_enu(position[0], position[1], position[2])
        message.pose.pose.position.x = ex
        message.pose.pose.position.y = ey
        message.pose.pose.position.z = ez
        roll, pitch, yaw = euler_ned_to_enu(att[0], att[1], att[2])
        qw, qx, qy, qz = euler_to_quaternion(roll, pitch, yaw)
        message.pose.pose.orientation = Quaternion(x=qx, y=qy, z=qz, w=qw)
        pose_cov = [0.0] * 36
        pose_cov[0] = pose_cov[7] = 0.05
        pose_cov[14] = 0.1
        pose_cov[35] = 0.05
        message.pose.covariance = pose_cov
        if odom is not None and now - odom_time <= 1.0:
            vx, vy, vz = body_frd_to_flu(odom[1], odom[2], odom[3])
        else:
            vx = vy = vz = 0.0
        if imu is not None and now - imu_time <= 0.5:
            wx, wy, wz = body_frd_rates_to_flu(imu[3], imu[4], imu[5])
        else:
            wx = wy = wz = 0.0
        message.twist.twist.linear.x = vx
        message.twist.twist.linear.y = vy
        message.twist.twist.linear.z = vz
        message.twist.twist.angular.x = wx
        message.twist.twist.angular.y = wy
        message.twist.twist.angular.z = wz
        twist_cov = [0.0] * 36
        twist_cov[0] = twist_cov[7] = 0.02
        twist_cov[14] = 0.02
        twist_cov[21] = twist_cov[28] = twist_cov[35] = 0.01
        message.twist.covariance = twist_cov
        self.pub_odom.publish(message)
        if self._tf_broadcaster is not None:
            transform = TransformStamped()
            transform.header.stamp = message.header.stamp
            transform.header.frame_id = self.odom_frame
            transform.child_frame_id = self.base_frame
            transform.transform.translation.x = ex
            transform.transform.translation.y = ey
            transform.transform.translation.z = ez
            transform.transform.rotation = message.pose.pose.orientation
            self._tf_broadcaster.sendTransform(transform)

    def _publish_battery(self, now):
        with self._lock:
            battery = self._battery
            battery_time = self._battery_time
        if battery is None or now - battery_time > 5.0:
            return
        voltages, current_centiamps, remaining = battery
        raw_voltage = voltages[0] if len(voltages) else -1
        if raw_voltage in (0, 0xFFFF):
            raw_voltage = -1
        voltage, current, percentage = mavlink_battery_to_ros(
            raw_voltage, current_centiamps, remaining
        )
        message = BatteryState()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = self.base_frame
        message.voltage = voltage
        message.current = -current if math.isfinite(current) else float("nan")
        message.charge = float("nan")
        message.capacity = float("nan")
        message.design_capacity = float("nan")
        message.percentage = (
            percentage / 100.0 if math.isfinite(percentage) else float("nan")
        )
        message.power_supply_status = BatteryState.POWER_SUPPLY_STATUS_UNKNOWN
        message.power_supply_health = BatteryState.POWER_SUPPLY_HEALTH_UNKNOWN
        message.power_supply_technology = BatteryState.POWER_SUPPLY_TECHNOLOGY_LIPO
        message.present = math.isfinite(voltage)
        self.pub_battery.publish(message)

    def _publish_fix(self, now):
        with self._lock:
            gps = self._gps
            gps_time = self._gps_time
        if gps is None or now - gps_time > 5.0:
            return
        fix_type, lat, lon, alt_mm, h_acc_mm, v_acc_mm, satellites = gps
        message = NavSatFix()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = "gps_link"
        message.status.service = NavSatStatus.SERVICE_GPS
        message.status.status = (
            NavSatStatus.STATUS_FIX if fix_type >= 2 else NavSatStatus.STATUS_NO_FIX
        )
        message.latitude = lat / 1e7
        message.longitude = lon / 1e7
        message.altitude = alt_mm / 1000.0
        if h_acc_mm > 0:
            message.position_covariance_type = (
                NavSatFix.COVARIANCE_TYPE_DIAGONAL_KNOWN
            )
            message.position_covariance[0] = (h_acc_mm / 1000.0) ** 2
            message.position_covariance[4] = (h_acc_mm / 1000.0) ** 2
            message.position_covariance[8] = (
                (v_acc_mm / 1000.0) ** 2 if v_acc_mm > 0 else float("nan")
            )
        else:
            message.position_covariance_type = NavSatFix.COVARIANCE_TYPE_UNKNOWN
        self.pub_fix.publish(message)

    def _publish_rc(self, now):
        with self._lock:
            rc = self._rc
            rc_time = self._rc_time
        if rc is None or now - rc_time > 1.0:
            return
        count, channels = rc
        message = Joy()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = self.base_frame
        count = max(0, min(int(count), 16))
        axes = []
        buttons = []
        for index in range(count):
            raw = channels[index]
            if index < 4:
                normalized = (
                    0.0 if raw in (0, 0xFFFF) else clamp((raw - 1500.0) / 500.0, 1.0)
                )
                axes.append(normalized)
            else:
                buttons.append(1 if raw > 1500 else 0)
        message.axes = axes if axes else [0.0, 0.0, 0.0, 0.0]
        message.buttons = buttons
        self.pub_rc.publish(message)

    def _publish_armed(self):
        with self._lock:
            armed = self._armed
        self.pub_armed.publish(Bool(data=armed))

    def _publish_statustext(self):
        with self._lock:
            items, self._statustext = self._statustext, []
        logger = self.get_logger()
        for severity, text in items:
            log = logger.info
            if severity <= 3:
                log = logger.error
            elif severity <= 5:
                log = logger.warning
            log("FC: %s" % text)
            self.pub_statustext.publish(String(data=text))

    def _named_value(self, name, now, max_age=3.0):
        entry = self._named.get(name)
        if entry is None or now - entry[1] > max_age:
            return None
        return entry[0]

    def _publish_diagnostics(self, now):
        array = DiagnosticArray()
        array.header.stamp = self.get_clock().now().to_msg()

        link = DiagnosticStatus()
        link.name = "mecanum_mavlink: serial"
        link.hardware_id = self.port
        pi_up = self._named_value("pi_up", now)
        esp_up = self._named_value("esp_up", now)
        esp_age = self._named_value("esp_age", now)
        rc_age = self._named_value("rc_age", now)
        state = self._named_value("state", now)
        source = self._named_value("src", now)
        fault = self._named_value("fault", now)
        cfg_rev = self._named_value("cfg_rev", now)
        if not self._link_ok(now):
            link.level = DiagnosticStatus.ERROR
            link.message = "no heartbeat from the flight controller"
        elif fault and int(round(fault)):
            link.level = DiagnosticStatus.ERROR
            link.message = "flight controller fault bits 0x%x" % int(round(fault))
        elif state is not None and int(round(state)) in (6, 7):
            link.level = DiagnosticStatus.ERROR
            link.message = "state %s" % STATE_NAMES.get(int(round(state)), state)
        elif not self._configured:
            link.level = DiagnosticStatus.WARN
            link.message = (
                "calibration invalid; set measured geometry in config/vehicle.yaml"
                if not self._calibration_valid()
                else "calibration not applied"
            )
        else:
            link.level = DiagnosticStatus.OK
            link.message = "link up, calibrated"
        link.values = [
            KeyValue(key="state", value=_fmt(state, STATE_NAMES)),
            KeyValue(key="source", value=_fmt(source, SOURCE_NAMES)),
            KeyValue(key="fault_bits", value=_fmt(fault)),
            KeyValue(key="config_revision", value=_fmt(cfg_rev)),
            KeyValue(key="pi_link", value=_fmt(pi_up)),
            KeyValue(key="esp_link", value=_fmt(esp_up)),
            KeyValue(key="esp_age_s", value=_fmt(esp_age)),
            KeyValue(key="rc_age_s", value=_fmt(rc_age)),
            KeyValue(key="heartbeat_age_s", value="%.2f" % (now - self._last_heartbeat)
                     if self._last_heartbeat else "never"),
            KeyValue(key="ping_rtt_ms", value="%.1f" % (self._rtt * 1e3)
                     if math.isfinite(self._rtt) else "n/a"),
            KeyValue(key="calibrated", value=str(self._configured)),
            KeyValue(key="armed", value=str(self._armed)),
            KeyValue(
                key="ready",
                value="n/a" if self.ready_line is None else str(self.ready_line.active),
            ),
            KeyValue(key="shutdown_pending", value=str(self._shutdown_pending)),
        ]
        array.status.append(link)

        rc_status = DiagnosticStatus()
        rc_status.name = "mecanum_mavlink: rc input"
        if rc_age is None or rc_age < 0:
            rc_status.level = DiagnosticStatus.WARN
            rc_status.message = "no RC sample received by the flight controller"
        elif rc_age > 0.6:
            rc_status.level = DiagnosticStatus.WARN
            rc_status.message = "RC input stale (%.1f s)" % rc_age
        else:
            rc_status.level = DiagnosticStatus.OK
            rc_status.message = "RC input live"
        rc_status.values = [KeyValue(key="rc_age_s", value=_fmt(rc_age))]
        array.status.append(rc_status)

        esp_status = DiagnosticStatus()
        esp_status.name = "mecanum_mavlink: esp32"
        if esp_up is None:
            esp_status.level = DiagnosticStatus.STALE
            esp_status.message = "ESP32 link state not reported"
        elif int(round(esp_up)) == 0 or (esp_age is not None and esp_age > 2.5):
            esp_status.level = DiagnosticStatus.WARN
            esp_status.message = "ESP32 not heard by the flight controller"
        else:
            esp_status.level = DiagnosticStatus.OK
            esp_status.message = "ESP32 link live"
        esp_status.values = [
            KeyValue(key="up", value=_fmt(esp_up)),
            KeyValue(key="age_s", value=_fmt(esp_age)),
        ]
        array.status.append(esp_status)

        array.status.append(self._electrical_status(now, fault))
        array.status.append(self._runtime_status(now))

        self.pub_diag.publish(array)

    def _electrical_status(self, now, fault):
        """Per-motor current, NTC, SOC and the stall mask from the firmware.

        Missing values stay "n/a": the firmware reports NAN for a channel it
        did not measure, and the host must not plot that as a real zero.
        """
        status = DiagnosticStatus()
        status.name = "mecanum_mavlink: electrical"
        status.hardware_id = self.port
        currents = [
            self._named_value("i_" + wheel, now) for wheel in ("fl", "fr", "rl", "rr")
        ]
        ntc = self._named_value("ntc_c", now)
        soc = self._named_value("soc", now)
        stall = self._named_value("stall", now)
        bits = int(round(fault)) if fault is not None else 0
        electrical = bits & ELECTRICAL_FAULTS
        if electrical:
            status.level = DiagnosticStatus.ERROR
            status.message = "electrical fault: %s" % ", ".join(
                name for bit, name in FAULT_BITS if bit & electrical
            )
        elif stall and int(round(stall)):
            status.level = DiagnosticStatus.WARN
            status.message = "motor stall latched: mask 0x%x" % int(round(stall))
        elif any(value is None for value in currents):
            status.level = DiagnosticStatus.WARN
            status.message = "motor current not reported by the flight controller"
        elif soc is None:
            status.level = DiagnosticStatus.WARN
            status.message = "battery state of charge not reported"
        else:
            status.level = DiagnosticStatus.OK
            status.message = "current and battery state reported"
        status.values = [
            KeyValue(key="i_fl_a", value=_fmt(currents[0])),
            KeyValue(key="i_fr_a", value=_fmt(currents[1])),
            KeyValue(key="i_rl_a", value=_fmt(currents[2])),
            KeyValue(key="i_rr_a", value=_fmt(currents[3])),
            KeyValue(key="ntc_c", value=_fmt(ntc)),
            KeyValue(key="soc", value=_fmt(soc)),
            KeyValue(key="stall_mask", value=_fmt(stall)),
            KeyValue(key="fault_bits", value="0x%02x" % bits),
            KeyValue(
                key="fault_names",
                value=",".join(name for bit, name in FAULT_BITS if bit & bits) or "none",
            ),
        ]
        return status

    def _runtime_status(self, now):
        """Stack high-water, loop jitter, CPU load and flash-save health.

        These are measurements, not targets: a missing value means the
        firmware has not reported it yet, and the page must show that.
        """
        status = DiagnosticStatus()
        status.name = "mecanum_mavlink: runtime"
        stacks = {
            name: self._named_value("stk_" + name, now)
            for name in ("safe", "ctrl", "host", "tele", "pow", "imu", "rx")
        }
        jitter_last = self._named_value("jit_us", now)
        jitter_max = self._named_value("jit_max", now)
        cpu = self._named_value("cpu_pct", now)
        cfg_seq = self._named_value("cfg_seq", now)
        cfg_err = self._named_value("cfg_err", now)
        measured = [v for v in stacks.values() if v is not None]
        low = sorted(
            (int(v), name)
            for name, v in stacks.items()
            if v is not None and v < LOW_STACK_WORDS
        )
        if not measured or cpu is None or jitter_last is None:
            status.level = DiagnosticStatus.STALE
            status.message = "runtime statistics not reported"
        elif cfg_err and int(round(cfg_err)):
            status.level = DiagnosticStatus.WARN
            status.message = "parameter flash save failed %d time(s)" % int(round(cfg_err))
        elif low:
            status.level = DiagnosticStatus.WARN
            status.message = "low stack: %s" % ", ".join(
                "%s %d words" % (name, words) for words, name in low
            )
        elif cpu > 80.0:
            status.level = DiagnosticStatus.WARN
            status.message = "CPU load %.0f%%" % cpu
        else:
            status.level = DiagnosticStatus.OK
            status.message = "runtime statistics nominal"
        status.values = [
            KeyValue(key="stack_free_%s_words" % name, value=_fmt(value))
            for name, value in stacks.items()
        ] + [
            KeyValue(key="jitter_last_us", value=_fmt(jitter_last)),
            KeyValue(key="jitter_max_us", value=_fmt(jitter_max)),
            KeyValue(key="cpu_percent", value=_fmt(cpu)),
            KeyValue(key="config_seq", value=_fmt(cfg_seq)),
            KeyValue(key="config_errors", value=_fmt(cfg_err)),
        ]
        return status

    # ------------------------------------------------------------------ teardown

    def destroy_node(self):
        self._stop.set()
        self._close_port()
        # Dropping READY is the interlock's "host going away" signal; release
        # the line only after it is low so the MCU never sees a floating input.
        if self.ready_line is not None:
            try:
                self.ready_line.close()
            except (RuntimeError, OSError) as exc:
                self.get_logger().error("cannot release READY: %s" % exc)
            self.ready_line = None
        super().destroy_node()


def _fmt(value, names=None):
    if value is None:
        return "n/a"
    if names and int(round(value)) in names:
        return names[int(round(value))]
    return "%.3f" % value


def main(args=None):
    rclpy.init(args=args)
    node = MavlinkBridge()
    executor = MultiThreadedExecutor(num_threads=2)
    executor.add_node(node)
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
