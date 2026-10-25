"""MAVLink ROS 2 bridge tests: pure conversions plus a PTY integration run.

The integration case opens a pseudo-terminal, runs a small MAVLink v2 fake
flight controller on the master side, and drives the production node through
the real pyserial/pymavlink path. No hardware is touched.

Run with the ROS 2 environment that has pymavlink installed:

    source /opt/ros/humble/setup.bash
    ~/.venvs/mecanum-ros2/bin/python -m unittest Code.tests.test_mavlink_ros -v
"""

import math
import os
from pathlib import Path
import sys
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Code/ros2_ws/src/mecanum_mavlink"))

from mecanum_mavlink.conversions import (  # noqa: E402
    body_frd_rates_to_flu,
    body_frd_to_flu,
    clamp,
    enu_twist_to_body_ned,
    euler_ned_to_enu,
    euler_to_quaternion,
    mavlink_battery_to_ros,
    ned_position_to_enu,
    quaternion_ned_to_enu,
    quaternion_to_euler,
)

try:
    from pymavlink.dialects.v20 import common as mav
    import rclpy
    from rclpy.executors import SingleThreadedExecutor
    from rclpy.parameter import Parameter
    from rclpy.qos import qos_profile_sensor_data
    from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
    from geometry_msgs.msg import Twist
    from nav_msgs.msg import Odometry
    from sensor_msgs.msg import BatteryState, Imu, Joy, NavSatFix, NavSatStatus
    from std_srvs.srv import SetBool, Trigger
    from mecanum_mavlink.bridge import CALIBRATION_TABLE, SETPOINT_TYPE_MASK, MavlinkBridge

    ROS_AVAILABLE = True
except ImportError:
    ROS_AVAILABLE = False

BODY_NED = 8
ARM_DISARM = 400
SHUTDOWN = 246
PI_SYS, PI_COMP = 255, 191
FC_SYS, FC_COMP = 1, 1


def _close(a, b, tol=1e-5):
    return abs(a - b) <= tol


class ConversionTests(unittest.TestCase):
    def test_quaternion_matches_euler_conversion(self):
        for roll, pitch, yaw in (
            (0.0, 0.0, 0.0),
            (0.0, 0.0, math.pi / 2),
            (math.pi / 2, 0.0, 0.0),
            (0.3, -0.2, 1.1),
            (-0.4, 0.5, -2.0),
        ):
            q = quaternion_ned_to_enu(euler_to_quaternion(roll, pitch, yaw))
            got = quaternion_to_euler(q)
            expect = euler_ned_to_enu(roll, pitch, yaw)
            for a, b in zip(got, expect):
                self.assertAlmostEqual(math.atan2(math.sin(a - b), math.cos(a - b)), 0.0, places=6)

    def test_vector_frames(self):
        self.assertEqual(ned_position_to_enu(2.0, 1.0, -3.0), (1.0, 2.0, 3.0))
        self.assertEqual(body_frd_to_flu(1.0, 2.0, 3.0), (1.0, -2.0, -3.0))
        self.assertEqual(body_frd_rates_to_flu(1.0, 2.0, 3.0), (1.0, -2.0, -3.0))
        self.assertEqual(enu_twist_to_body_ned(1.0, 2.0, 0.5), (1.0, -2.0, -0.5))

    def test_clamp_and_battery_unknowns(self):
        self.assertEqual(clamp(5.0, 1.0), 1.0)
        self.assertEqual(clamp(-5.0, 1.0), -1.0)
        voltage, current, percentage = mavlink_battery_to_ros(-1, -1, -1)
        self.assertTrue(math.isnan(voltage))
        self.assertTrue(math.isnan(current))
        self.assertTrue(math.isnan(percentage))
        voltage, current, percentage = mavlink_battery_to_ros(12600, -1, 60)
        self.assertAlmostEqual(voltage, 12.6, places=6)
        self.assertTrue(math.isnan(current))
        self.assertAlmostEqual(percentage, 60.0, places=6)


class _FdSink:
    def __init__(self, fd):
        self.fd = fd

    def write(self, data):
        try:
            os.write(self.fd, data)
        except OSError:
            pass
        return len(data)


class FakeReadyLine:
    """Records READY edges instead of claiming a real GPIO character device."""

    def __init__(self):
        self.active = False
        self.closed = False

    def set(self, active):
        self.active = bool(active)

    def close(self):
        self.active = False
        self.closed = True


class FakeFlightController(threading.Thread):
    """Answers the bridge like the STM32 firmware does, at a quiet rate."""

    def __init__(self, fd):
        super().__init__(daemon=True, name="fake-fc")
        self.fd = fd
        self.mav = mav.MAVLink(_FdSink(fd), srcSystem=FC_SYS, srcComponent=FC_COMP)
        self.mav.robust_parsing = True
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.pi_alive = False
        self.params = {name: 0.0 for name, _, _, _ in CALIBRATION_TABLE}
        self.armed = False
        self.state = 1
        self.arming_since = 0.0
        self.setpoints = []
        self.shutdowns = []
        self.next_heartbeat = 0.0
        self.next_fast = 0.0
        self.next_slow = 0.0
        self.next_named = 0.0

    def params_valid(self):
        p = self.params
        return (
            p["RADIUS_M"] >= 0.01
            and p["HALF_LEN_M"] >= 0.01
            and p["HALF_WID_M"] >= 0.01
            and p["COUNTS_REV"] > 0
            and p["DUTY_LIMIT"] > 0
            and p["SPEED_TAU"] >= 0.001
        )

    def _heartbeat(self):
        base = mav.MAV_MODE_FLAG_SAFETY_ARMED if self.armed else 0
        status = mav.MAV_STATE_ACTIVE if self.armed else mav.MAV_STATE_STANDBY
        self.mav.heartbeat_send(
            mav.MAV_TYPE_GROUND_ROVER,
            mav.MAV_AUTOPILOT_GENERIC,
            base,
            0,
            status,
        )

    def _send_param(self, name):
        index = [row[0] for row in CALIBRATION_TABLE].index(name)
        is_int = CALIBRATION_TABLE[index][3]
        self.mav.param_value_send(
            name.encode("ascii"),
            self.params[name],
            mav.MAV_PARAM_TYPE_INT32 if is_int else mav.MAV_PARAM_TYPE_REAL32,
            len(CALIBRATION_TABLE),
            index,
        )

    def _handle(self, msg):
        now = time.monotonic()
        kind = msg.get_type()
        if kind == "HEARTBEAT" and msg.get_srcSystem() == PI_SYS:
            self.pi_alive = True
        elif kind == "PARAM_SET":
            if self.state == 1:
                self.params[msg.param_id] = msg.param_value
            self._send_param(msg.param_id)
        elif kind == "PARAM_REQUEST_LIST":
            for name in self.params:
                self._send_param(name)
        elif kind == "PARAM_REQUEST_READ":
            name = msg.param_id if msg.param_index < 0 else list(self.params)[msg.param_index]
            if name in self.params:
                self._send_param(name)
        elif kind == "COMMAND_LONG" and msg.command == ARM_DISARM:
            arm = msg.param1 >= 0.5
            ok = (not arm) or self.params_valid()
            if ok:
                self.armed = arm
                self.state = 2 if arm else 1
                self.arming_since = now
            self.mav.command_ack_send(
                ARM_DISARM,
                mav.MAV_RESULT_ACCEPTED if ok else mav.MAV_RESULT_DENIED,
                target_system=PI_SYS,
                target_component=PI_COMP,
            )
        elif kind == "COMMAND_LONG" and msg.command == SHUTDOWN:
            with self.lock:
                self.shutdowns.append((msg.param1, msg.param2))
            # Mirrors the firmware: companion shutdown only, disarmed only.
            ok = not self.armed and msg.param1 == 0.0 and msg.param2 == 2.0
            self.mav.command_ack_send(
                SHUTDOWN,
                mav.MAV_RESULT_ACCEPTED if ok else mav.MAV_RESULT_DENIED,
                target_system=PI_SYS,
                target_component=PI_COMP,
            )
        elif kind == "SET_POSITION_TARGET_LOCAL_NED":
            with self.lock:
                self.setpoints.append(
                    (
                        msg.coordinate_frame,
                        msg.type_mask,
                        msg.vx,
                        msg.vy,
                        msg.vz,
                        msg.yaw_rate,
                    )
                )
                del self.setpoints[:-256]
        elif kind == "PING":
            self.mav.ping_send(msg.time_usec, msg.seq, PI_SYS, PI_COMP)

    def _telemetry(self, now):
        if not self.pi_alive:
            return
        if now >= self.next_heartbeat:
            self.next_heartbeat = now + 0.1
            self._heartbeat()
        if self.state == 2 and now - self.arming_since > 0.03:
            self.state = 5
        if now >= self.next_fast:
            self.next_fast = now + 0.02
            self.mav.attitude_send(
                int(now * 1000), 0.2, 0.1, 0.3, 0.01, 0.02, 0.03
            )
            self.mav.highres_imu_send(
                int(now * 1e6), 1.0, 2.0, -9.81, 0.01, 0.02, 0.03,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 25.0, 0x1FF,
            )
        if now >= self.next_slow:
            self.next_slow = now + 0.1
            self.mav.local_position_ned_send(int(now * 1000), 2.0, 1.0, 0.0, 0.0, 0.0, 0.0)
            self.mav.odometry_send(
                int(now * 1e6), mav.MAV_FRAME_LOCAL_NED, mav.MAV_FRAME_BODY_FRD,
                2.0, 1.0, 0.0, [1.0, 0.0, 0.0, 0.0],
                1.0, 0.5, 0.0, 0.01, 0.02, 0.03,
                [0.0] * 21, [0.0] * 21,
            )
            voltages = [12600] + [0xFFFF] * 9
            self.mav.battery_status_send(
                0, mav.MAV_BATTERY_FUNCTION_ALL, mav.MAV_BATTERY_TYPE_LIPO,
                -1, voltages, -1, -1, -1, 60,
            )
            self.mav.gps_raw_int_send(
                int(now * 1e6), 3, 312300000, 1214700000, 12000,
                150, 250, 0, 0, 12, 12000, 1500, 2500, 0, 0, 0,
            )
            self.mav.rc_channels_send(
                int(now * 1000), 8, 1600, 1400, 1500, 2000,
                1600, 1000, 1500, 2000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            )
        if now >= self.next_named:
            self.next_named = now + 0.1
            for name, value in (
                ("pi_up", 1.0),
                ("esp_up", 1.0),
                ("esp_age", 0.2),
                ("rc_age", 0.1),
                ("state", float(self.state)),
                ("src", 0.0),
                ("fault", 0.0),
                ("cfg_rev", 3.0),
                # Electrical health and runtime evidence mirrored by the
                # firmware's named-value block.
                ("i_fl", 0.12), ("i_fr", 0.11), ("i_rl", 0.13), ("i_rr", 0.10),
                ("ntc_c", 31.5), ("soc", 0.55), ("stall", 0.0),
                ("stk_safe", 300.0), ("stk_ctrl", 400.0), ("stk_host", 500.0),
                ("stk_tele", 500.0), ("stk_pow", 300.0), ("stk_imu", 300.0),
                ("stk_rx", 400.0),
                ("jit_us", 30.0), ("jit_max", 80.0), ("cpu_pct", 25.0),
                ("cfg_seq", 2.0), ("cfg_err", 0.0),
            ):
                self.mav.named_value_float_send(int(now * 1000), name.encode("ascii"), value)

    def run(self):
        while not self.stop.is_set():
            try:
                data = os.read(self.fd, 4096)
            except BlockingIOError:
                data = b""
            except OSError:
                break
            if data:
                for msg in self.mav.parse_buffer(data) or ():
                    self._handle(msg)
            self._telemetry(time.monotonic())
            time.sleep(0.002)


@unittest.skipUnless(ROS_AVAILABLE, "Run with the mecanum-ros2 Python environment")
class BridgeIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.master, cls.slave = os.openpty()
        os.set_blocking(cls.master, False)
        cls.port = os.ttyname(cls.slave)
        cls.fake = FakeFlightController(cls.master)
        cls.fake.start()
        if not rclpy.ok():
            rclpy.init(args=[])
            cls.owned_context = True
        else:
            cls.owned_context = False

    @classmethod
    def tearDownClass(cls):
        cls.fake.stop.set()
        cls.fake.join(timeout=1.0)
        try:
            os.close(cls.master)
        except OSError:
            pass
        try:
            os.close(cls.slave)
        except OSError:
            pass
        if cls.owned_context and rclpy.ok():
            rclpy.shutdown()

    def setUp(self):
        overrides = [Parameter("port", value=self.port)]
        for name, suffix, default, _ in CALIBRATION_TABLE:
            value = default
            if name == "RADIUS_M":
                value = 0.05
            elif name == "HALF_LEN_M":
                value = 0.15
            elif name == "HALF_WID_M":
                value = 0.12
            elif name == "COUNTS_REV":
                value = 2048
            overrides.append(Parameter("calibration." + suffix, value=value))
        self.node = MavlinkBridge(parameter_overrides=overrides)
        # The READY claim needs a real gpiochip; the test records the edges.
        self.ready = FakeReadyLine()
        self.node.ready_line = self.ready
        self.observer = rclpy.create_node("mavlink_test_observer")
        self.executor = SingleThreadedExecutor()
        self.executor.add_node(self.node)
        self.executor.add_node(self.observer)
        self.received = {"imu": [], "odom": [], "battery": [], "fix": [], "rc": [], "diag": []}
        self._subscriptions = [
            self.observer.create_subscription(
                Imu, "imu/data", lambda m: self.received["imu"].append(m), qos_profile_sensor_data
            ),
            self.observer.create_subscription(
                Odometry, "odom", lambda m: self.received["odom"].append(m), 10
            ),
            self.observer.create_subscription(
                BatteryState, "battery", lambda m: self.received["battery"].append(m), 10
            ),
            self.observer.create_subscription(
                NavSatFix, "fix", lambda m: self.received["fix"].append(m), 10
            ),
            self.observer.create_subscription(
                Joy, "rc/channels", lambda m: self.received["rc"].append(m), 10
            ),
            self.observer.create_subscription(
                DiagnosticArray,
                "/diagnostics",
                lambda m: self.received["diag"].append(m),
                10,
            ),
        ]

    def tearDown(self):
        self.executor.remove_node(self.node)
        self.executor.remove_node(self.observer)
        self.node.destroy_node()
        self.observer.destroy_node()
        del self._subscriptions

    def spin_until(self, predicate, timeout=8.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.executor.spin_once(timeout_sec=0.02)
            if predicate():
                return True
        return False

    def call_service(self, client, request, timeout=5.0):
        future = client.call_async(request)
        if not self.spin_until(future.done, timeout):
            raise AssertionError("service call timed out")
        return future.result()

    def diagnostic(self, suffix):
        """Newest status block whose name ends with ``suffix``."""
        for array in reversed(self.received["diag"]):
            for status in array.status:
                if status.name.endswith(suffix):
                    return status
        return None

    def wait_for_status(self, suffix, level, timeout=8.0):
        def reached():
            status = self.diagnostic(suffix)
            return status is not None and status.level == level

        self.assertTrue(
            self.spin_until(reached, timeout),
            "%s never reached level %r: %r"
            % (suffix, level, self.diagnostic(suffix)),
        )
        return self.diagnostic(suffix)

    def test_shutdown_is_refused_while_armed_and_drops_ready_when_accepted(self):
        self.assertTrue(self.spin_until(lambda: self.node._configured, 8.0))
        # READY follows a live link; the firmware needs it to grant PI authority.
        self.assertTrue(self.spin_until(lambda: self.ready.active, 3.0))

        arm = self.observer.create_client(SetBool, "/mecanum_mavlink/arm")
        shutdown = self.observer.create_client(
            Trigger, "/mecanum_mavlink/shutdown_pi"
        )
        self.assertTrue(arm.wait_for_service(timeout_sec=2.0))
        self.assertTrue(shutdown.wait_for_service(timeout_sec=2.0))

        self.assertTrue(self.call_service(arm, SetBool.Request(data=True)).success)
        self.assertTrue(self.spin_until(lambda: self.node._armed, 3.0))
        with self.fake.lock:
            self.fake.shutdowns.clear()
        response = self.call_service(shutdown, Trigger.Request())
        self.assertFalse(response.success)
        self.assertIn("disarm", response.message)
        with self.fake.lock:
            self.assertEqual(self.fake.shutdowns, [])
        self.assertTrue(self.ready.active)

        self.assertTrue(self.call_service(arm, SetBool.Request(data=False)).success)
        self.assertTrue(self.spin_until(lambda: not self.node._armed, 3.0))
        response = self.call_service(shutdown, Trigger.Request())
        self.assertTrue(response.success, response.message)
        with self.fake.lock:
            self.assertEqual(self.fake.shutdowns, [(0.0, 2.0)])
        self.assertTrue(self.node._shutdown_pending)
        self.assertFalse(self.ready.active)
        # The timer must not raise READY again once a shutdown is pending.
        self.executor.spin_once(timeout_sec=0.1)
        self.assertFalse(self.ready.active)

        with self.fake.lock:
            count = len(self.fake.shutdowns)
        response = self.call_service(shutdown, Trigger.Request())
        self.assertFalse(response.success)
        with self.fake.lock:
            self.assertEqual(len(self.fake.shutdowns), count)

    def test_diagnostics_report_electrical_and_runtime_measurements(self):
        self.assertTrue(self.spin_until(lambda: self.node._configured, 8.0))
        electrical = self.wait_for_status("electrical", DiagnosticStatus.OK)
        values = {kv.key: kv.value for kv in electrical.values}
        self.assertAlmostEqual(float(values["i_fl_a"]), 0.12, places=5)
        self.assertAlmostEqual(float(values["i_rr_a"]), 0.10, places=5)
        self.assertAlmostEqual(float(values["ntc_c"]), 31.5, places=5)
        self.assertAlmostEqual(float(values["soc"]), 0.55, places=5)
        self.assertEqual(values["stall_mask"], "0.000")
        self.assertEqual(values["fault_names"], "none")

        runtime = self.wait_for_status("runtime", DiagnosticStatus.OK)
        values = {kv.key: kv.value for kv in runtime.values}
        self.assertEqual(values["stack_free_ctrl_words"], "400.000")
        self.assertEqual(values["jitter_max_us"], "80.000")
        self.assertEqual(values["cpu_percent"], "25.000")
        self.assertEqual(values["config_errors"], "0.000")
        self.assertEqual(values["config_seq"], "2.000")

    def test_handshake_telemetry_arming_and_setpoints(self):
        # Calibration handshake: every PARAM_SET is echoed and accepted.
        self.assertTrue(
            self.spin_until(lambda: self.node._configured, 8.0),
            "bridge did not finish the parameter handshake",
        )
        self.assertTrue(
            self.spin_until(
                lambda: all(self.received[key] for key in self.received), 8.0
            ),
            "telemetry topics did not arrive",
        )

        imu = self.received["imu"][-1]
        roll, pitch, yaw = euler_ned_to_enu(0.2, 0.1, 0.3)
        qw, qx, qy, qz = euler_to_quaternion(roll, pitch, yaw)
        dot = abs(
            imu.orientation.w * qw
            + imu.orientation.x * qx
            + imu.orientation.y * qy
            + imu.orientation.z * qz
        )
        self.assertAlmostEqual(dot, 1.0, places=5)
        self.assertAlmostEqual(imu.linear_acceleration.x, 1.0, places=4)
        self.assertAlmostEqual(imu.linear_acceleration.y, -2.0, places=4)
        self.assertAlmostEqual(imu.linear_acceleration.z, 9.81, places=4)
        self.assertAlmostEqual(imu.angular_velocity.x, 0.01, places=5)
        self.assertAlmostEqual(imu.angular_velocity.y, -0.02, places=5)
        self.assertAlmostEqual(imu.angular_velocity.z, -0.03, places=5)

        odom = self.received["odom"][-1]
        self.assertAlmostEqual(odom.pose.pose.position.x, 1.0, places=5)
        self.assertAlmostEqual(odom.pose.pose.position.y, 2.0, places=5)
        self.assertAlmostEqual(odom.pose.pose.position.z, 0.0, places=5)
        self.assertAlmostEqual(odom.twist.twist.linear.x, 1.0, places=5)
        self.assertAlmostEqual(odom.twist.twist.linear.y, -0.5, places=5)
        self.assertAlmostEqual(odom.twist.twist.angular.z, -0.03, places=5)

        battery = self.received["battery"][-1]
        self.assertAlmostEqual(battery.voltage, 12.6, places=3)
        self.assertAlmostEqual(battery.percentage, 0.6, places=4)
        self.assertTrue(math.isnan(battery.current))

        fix = self.received["fix"][-1]
        self.assertAlmostEqual(fix.latitude, 31.23, places=6)
        self.assertAlmostEqual(fix.longitude, 121.47, places=6)
        self.assertAlmostEqual(fix.altitude, 12.0, places=3)
        self.assertAlmostEqual(fix.position_covariance[0], 1.5 ** 2, places=5)
        self.assertEqual(fix.status.status, NavSatStatus.STATUS_FIX)

        rc = self.received["rc"][-1]
        self.assertEqual(len(rc.axes), 4)
        self.assertAlmostEqual(rc.axes[0], 0.2, places=5)
        self.assertAlmostEqual(rc.axes[1], -0.2, places=5)
        self.assertAlmostEqual(rc.axes[2], 0.0, places=5)
        self.assertAlmostEqual(rc.axes[3], 1.0, places=5)
        self.assertEqual(list(rc.buttons), [1, 0, 0, 1])

        def live_diagnostics():
            for array in self.received["diag"]:
                for status in array.status:
                    if status.name.endswith("serial"):
                        values = {kv.key: kv.value for kv in status.values}
                        if values.get("heartbeat_age_s") not in (None, "never"):
                            return status
            return None

        self.assertTrue(self.spin_until(lambda: live_diagnostics() is not None, 3.0))
        diagnostics = live_diagnostics()
        self.assertIsNotNone(diagnostics)
        link = diagnostics
        values = {kv.key: kv.value for kv in link.values}
        self.assertEqual(
            link.level,
            DiagnosticStatus.OK,
            "%s %s" % (link.message, values),
        )
        self.assertEqual(values["state"], "DISARMED")
        self.assertEqual(values["source"], "pi")
        self.assertEqual(values["esp_link"], "1.000")
        self.assertEqual(values["calibrated"], "True")

        client = self.observer.create_client(SetBool, "/mecanum_mavlink/arm")
        self.assertTrue(client.wait_for_service(timeout_sec=2.0))
        response = self.call_service(client, SetBool.Request(data=True))
        self.assertTrue(response.success, response.message)
        self.assertTrue(self.spin_until(lambda: self.node._armed, 3.0))

        cmd = self.observer.create_publisher(Twist, "cmd_vel", 10)
        command = Twist()
        command.linear.x = 0.5
        command.linear.y = 0.25
        command.angular.z = 0.4
        command.linear.z = 1.0  # must be ignored: the base has no vertical axis
        cmd.publish(command)

        def latest_setpoint():
            with self.fake.lock:
                return self.fake.setpoints[-1] if self.fake.setpoints else None

        self.assertTrue(self.spin_until(lambda: latest_setpoint() is not None, 3.0))
        self.assertTrue(
            self.spin_until(
                lambda: any(
                    _close(sp[2], 0.5, 1e-4)
                    and _close(sp[3], -0.25, 1e-4)
                    and _close(sp[5], -0.4, 1e-4)
                    and sp[0] == BODY_NED
                    and sp[1] == SETPOINT_TYPE_MASK
                    for sp in self.fake.setpoints
                ),
                3.0,
            ),
            "cmd_vel was not converted to a BODY_NED velocity setpoint",
        )

        response = self.call_service(client, SetBool.Request(data=False))
        self.assertTrue(response.success, response.message)
        self.assertTrue(self.spin_until(lambda: not self.node._armed, 3.0))


if __name__ == "__main__":
    unittest.main()
