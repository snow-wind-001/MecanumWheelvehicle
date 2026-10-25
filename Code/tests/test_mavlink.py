"""MAVLink v2 adapter and attitude-estimator tests against the production C code.

Run with the ROS 2 environment that has pymavlink installed:

    ~/.venvs/mecanum-ros2/bin/python -m unittest Code.tests.test_mavlink -v
"""
import ctypes as C
import math
from pathlib import Path
import subprocess
import tempfile
import unittest

try:
    from pymavlink.dialects.v20 import common as mav
except ImportError:  # system Python may not carry pymavlink
    mav = None

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'Code/MecanumB_Control/App'
MAVLINK = ROOT / 'Code/MecanumB_Control/Middlewares/Third_Party/MAVLink'

VL_DISARMED, VL_ARMING, VL_WIFI_ACTIVE, VL_PI_ACTIVE, VL_FAILSAFE = 1, 2, 4, 5, 6
BODY_NED = 8
IGNORE_POS = 0x7
IGNORE_ACC = 0x1C0
IGNORE_YAW = 0x400

NOW = 1_000_000

# Measured geometry and bench limits the vehicle refuses to arm without.
PARAMS = [
    ('RADIUS_M', .05, 9), ('HALF_LEN_M', .15, 9), ('HALF_WID_M', .12, 9),
    ('COUNTS_REV', 2048, 6),
    ('ENC_SIGN0', 1, 6), ('ENC_SIGN1', 1, 6), ('ENC_SIGN2', 1, 6), ('ENC_SIGN3', 1, 6),
    ('MOT_SIGN0', 1, 6), ('MOT_SIGN1', 1, 6), ('MOT_SIGN2', 1, 6), ('MOT_SIGN3', 1, 6),
    ('MAX_WHEEL', 20., 9), ('MAX_ACCEL', 100., 9), ('KP', .1, 9), ('KI', .2, 9),
    ('FF', .02, 9), ('DUTY_LIMIT', .5, 9), ('SPEED_TAU', .01, 9), ('REVERSAL', .5, 9),
]


class Sink:
    def __init__(self):
        self.data = bytearray()

    def write(self, data):
        self.data += data
        return len(data)


def encode(call, sysid=250, compid=1):
    link = mav.MAVLink(Sink(), srcSystem=sysid, srcComponent=compid)
    return call(link).pack(link)


def decode(data):
    sink = Sink()
    parser = mav.MAVLink(sink)
    out = []
    for byte in data:
        msg = parser.parse_char(bytes([byte]))
        if msg:
            out.append(msg)
    return out


@unittest.skipIf(mav is None, 'pymavlink is not installed')
class MavlinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        sources = [str(APP / 'src' / (name + '.c')) for name in (
            'vehicle_mavlink', 'vehicle_fusion', 'vehicle_link', 'vehicle_control', 'vehicle_protocol')]
        target = str(Path(cls.temp.name) / 'mavlink_fixture.so')
        subprocess.run(['gcc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-Wno-address-of-packed-member', *getattr(cls, 'CFLAGS', []),
                        '-I' + str(APP / 'include'), '-I' + str(MAVLINK),
                        str(ROOT / 'Code/tests/mavlink_fixture.c'), *sources, '-lm', '-o', target],
                       check=True, capture_output=True)
        cls.lib = C.CDLL(target)
        L = cls.lib
        L.mav_create.restype = C.c_void_p
        L.mav_destroy.argtypes = [C.c_void_p]
        L.mav_feed.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_size_t, C.c_uint64]
        L.mav_lost.argtypes = [C.c_void_p, C.c_uint, C.c_uint64]
        L.mav_tick.argtypes = [C.c_void_p, C.c_uint64]
        L.mav_pop.argtypes = [C.c_void_p, C.c_uint, C.c_void_p, C.c_int]
        L.mav_pop.restype = C.c_int
        L.mav_state.argtypes = [C.c_void_p]
        L.mav_state.restype = C.c_uint
        L.mav_source.argtypes = [C.c_void_p]
        L.mav_source.restype = C.c_uint
        L.mav_alive.argtypes = [C.c_void_p, C.c_uint]
        L.mav_alive.restype = C.c_int
        L.mav_maintenance.argtypes = [C.c_void_p, C.c_int]
        L.mav_disarm.argtypes = [C.c_void_p, C.c_uint64]
        L.mav_sbus.argtypes = [C.c_void_p, C.c_uint64, C.c_uint64, C.c_int, C.c_int]
        L.mav_last_command.argtypes = [C.c_void_p]
        L.mav_last_command.restype = C.c_uint64
        L.mav_power_inhibit.argtypes = [C.c_void_p]
        L.mav_power_inhibit.restype = C.c_int
        L.mav_shutdown_requested.argtypes = [C.c_void_p]
        L.mav_shutdown_requested.restype = C.c_int
        L.mav_config_seq.argtypes = [C.c_void_p]
        L.mav_config_seq.restype = C.c_uint32
        L.mav_set_health.argtypes = [C.c_void_p, C.POINTER(C.c_float), C.c_float, C.c_int,
                                     C.c_float, C.c_int]
        L.mav_rx_messages.argtypes = [C.c_void_p, C.c_uint]
        L.mav_rx_messages.restype = C.c_uint32
        L.mav_body.argtypes = [C.c_void_p, C.POINTER(C.c_float)]
        L.mav_set_wheel.argtypes = [C.c_void_p, C.c_float, C.c_float, C.c_float]
        L.mav_sensor_imu.argtypes = [C.c_void_p] + [C.c_float] * 6 + [C.c_uint64]
        L.mav_sensor_imu.restype = C.c_int
        L.mav_fusion_reset.argtypes = [C.c_void_p]
        L.mav_fusion_update.argtypes = [C.c_void_p, C.POINTER(C.c_float), C.POINTER(C.c_float), C.c_float]
        L.mav_fusion_euler.argtypes = [C.c_void_p, C.POINTER(C.c_float)]
        L.mav_fusion_yaw_ned.argtypes = [C.c_void_p]
        L.mav_fusion_yaw_ned.restype = C.c_float

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.dev = self.lib.mav_create()
        self.assertTrue(self.dev)

    def tearDown(self):
        self.lib.mav_destroy(self.dev)

    def feed(self, wire, now, channel=0):
        self.lib.mav_feed(self.dev, channel, wire, len(wire), now)

    def heartbeat(self, now, channel=0):
        self.feed(encode(lambda m: m.heartbeat_encode(18, 8, 0, 0, 4)), now, channel)
        self.lib.mav_tick(self.dev, now)

    def arm(self, now, channel=0, sysid=250):
        self.feed(encode(lambda m: m.command_long_encode(
            1, 1, mav.MAV_CMD_COMPONENT_ARM_DISARM, 0, 1, 0, 0, 0, 0, 0, 0), sysid), now, channel)
        self.lib.mav_tick(self.dev, now)

    def param_set(self, name, value, ptype, now, channel=0):
        pid = name.encode().ljust(16, b'\0')
        self.feed(encode(lambda m: m.param_set_encode(1, 1, pid, value, ptype)), now, channel)

    def shutdown(self, now, channel=0, param1=0., param2=2.):
        self.feed(encode(lambda m: m.command_long_encode(
            1, 1, mav.MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN, 0,
            param1, param2, 0, 0, 0, 0, 0)), now, channel)
        self.lib.mav_tick(self.dev, now)

    def configure(self, now, channel=0):
        for name, value, ptype in PARAMS:
            self.param_set(name, value, ptype, now, channel)
        self.lib.mav_tick(self.dev, now)

    def setpoint(self, now, vx, vy, yaw_rate, channel=0, frame=BODY_NED,
                 target_system=1, target_component=1):
        mask = IGNORE_POS | IGNORE_ACC | IGNORE_YAW
        self.feed(encode(lambda m: m.set_position_target_local_ned_encode(
            0, target_system, target_component, frame, mask,
            0, 0, 0, vx, vy, 0, 0, 0, 0, 0, yaw_rate)), now, channel)

    def body(self):
        out = (C.c_float * 3)()
        self.lib.mav_body(self.dev, out)
        return list(out)

    def drain(self, channel=0):
        buf = C.create_string_buffer(16384)
        n = self.lib.mav_pop(self.dev, channel, buf, len(buf))
        return decode(buf.raw[:n])

    def state(self):
        return self.lib.mav_state(self.dev)

    def test_flash_maintenance_rejects_arm_until_released(self):
        self.heartbeat(NOW)
        self.configure(NOW + 1000)
        self.lib.mav_maintenance(self.dev, 1)
        self.arm(NOW + 2000)
        self.assertEqual(self.state(), VL_DISARMED)
        self.lib.mav_maintenance(self.dev, 0)
        self.arm(NOW + 3000)
        self.assertEqual(self.state(), VL_ARMING)

    def test_heartbeat_sets_liveness_and_emits_telemetry(self):
        self.heartbeat(NOW)
        self.assertTrue(self.lib.mav_alive(self.dev, 0))
        self.assertFalse(self.lib.mav_alive(self.dev, 1))
        messages = self.drain()
        kinds = {m.get_type() for m in messages}
        self.assertTrue({'HEARTBEAT', 'SYS_STATUS', 'ATTITUDE', 'HIGHRES_IMU', 'LOCAL_POSITION_NED',
                         'ODOMETRY', 'RC_CHANNELS', 'BATTERY_STATUS', 'GPS_RAW_INT', 'VFR_HUD'}
                        <= kinds, kinds)
        heartbeat = next(m for m in messages if m.get_type() == 'HEARTBEAT')
        self.assertEqual(heartbeat.type, mav.MAV_TYPE_GROUND_ROVER)
        self.assertEqual(heartbeat.autopilot, mav.MAV_AUTOPILOT_GENERIC)
        self.assertEqual(heartbeat.system_status, mav.MAV_STATE_STANDBY)
        self.assertEqual(self.lib.mav_rx_messages(self.dev, 0), 1)

    def test_arm_promotes_then_motion_and_heartbeat_timeout_failsafe(self):
        self.heartbeat(NOW)
        self.configure(NOW + 5_000)
        self.arm(NOW + 10_000)
        self.assertEqual(self.state(), VL_ARMING)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        self.assertEqual(self.state(), VL_PI_ACTIVE)
        self.assertEqual(self.lib.mav_source(self.dev), 0)
        self.setpoint(NOW + 50_000, .5, -.25, .5)
        self.lib.mav_tick(self.dev, NOW + 50_000)
        body = self.body()
        self.assertAlmostEqual(body[0], .5, places=5)
        self.assertAlmostEqual(body[1], -.25, places=5)
        self.assertAlmostEqual(body[2], -.5, places=5)  # body yaw is CCW-positive

        # Keep the link alive with 4 Hz traffic, then let 2 s of silence expire.
        t = NOW + 50_000
        for _ in range(8):
            t += 250_000
            self.heartbeat(t)
            self.setpoint(t, .3, 0., 0.)
            self.lib.mav_tick(self.dev, t)
        self.drain()
        self.lib.mav_tick(self.dev, t + 2_100_000)
        self.assertFalse(self.lib.mav_alive(self.dev, 0))
        self.assertEqual(self.state(), VL_FAILSAFE)
        self.assertEqual(self.body(), [0.0, 0.0, 0.0])

    def test_arm_rejected_before_heartbeat_and_motion_rejected_without_arm(self):
        self.configure(NOW)
        self.arm(NOW)
        self.assertEqual(self.state(), VL_DISARMED)
        self.heartbeat(NOW + 10_000)
        self.setpoint(NOW + 20_000, 1., 0., 0.)
        self.lib.mav_tick(self.dev, NOW + 20_000)
        self.assertEqual(self.state(), VL_DISARMED)
        self.assertEqual(self.body(), [0.0, 0.0, 0.0])
        self.assertEqual(self.lib.mav_last_command(self.dev), 0)

    def test_param_protocol_configures_reads_back_and_is_read_only_armed(self):
        self.heartbeat(NOW)
        self.drain()
        self.feed(encode(lambda m: m.param_request_list_encode(1, 1)), NOW + 1_000)
        values = [m for m in self.drain() if m.get_type() == 'PARAM_VALUE']
        self.assertEqual(len(values), len(PARAMS))
        self.assertEqual(values[0].param_count, len(PARAMS))
        self.assertEqual(values[0].param_id, 'RADIUS_M')
        self.assertEqual(values[0].param_value, 0.)

        self.param_set('RADIUS_M', .05, 9, NOW + 2_000)
        echo = next(m for m in self.drain() if m.get_type() == 'PARAM_VALUE')
        self.assertAlmostEqual(echo.param_value, .05, places=6)
        # One parameter is not a complete calibration.
        self.arm(NOW + 3_000)
        self.assertEqual(self.state(), VL_DISARMED)

        self.configure(NOW + 4_000)
        self.arm(NOW + 5_000)
        self.lib.mav_tick(self.dev, NOW + 30_000)
        self.assertEqual(self.state(), VL_PI_ACTIVE)
        self.param_set('RADIUS_M', .2, 9, NOW + 40_000)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        echo = next(m for m in self.drain() if m.get_type() == 'PARAM_VALUE')
        self.assertAlmostEqual(echo.param_value, .05, places=6)
        self.assertEqual(self.state(), VL_PI_ACTIVE)

        self.feed(encode(lambda m: m.param_request_read_encode(1, 1, b'COUNTS_REV'.ljust(16, b'\0'), -1)),
                  NOW + 50_000)
        echo = next(m for m in self.drain() if m.get_type() == 'PARAM_VALUE')
        self.assertEqual(echo.param_id, 'COUNTS_REV')
        self.assertEqual(echo.param_value, 2048.)

    def test_setpoint_target_filter_and_range(self):
        self.heartbeat(NOW)
        self.configure(NOW + 5_000)
        self.arm(NOW + 10_000)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        self.setpoint(NOW + 50_000, 1., 0., 0., target_system=2)
        self.setpoint(NOW + 55_000, 1., 0., 0., target_component=9)
        self.lib.mav_tick(self.dev, NOW + 60_000)
        self.assertEqual(self.body(), [0.0, 0.0, 0.0])
        # Out-of-range velocity is refused without changing the old command.
        self.setpoint(NOW + 65_000, .5, 0., 0.)
        self.lib.mav_tick(self.dev, NOW + 65_000)
        self.setpoint(NOW + 70_000, 9., 0., 0.)
        self.lib.mav_tick(self.dev, NOW + 70_000)
        self.assertAlmostEqual(self.body()[0], .5, places=5)

    def test_ping_and_timesync_reply_on_requesting_channel(self):
        self.heartbeat(NOW)
        self.drain()
        self.feed(encode(lambda m: m.ping_encode(123456, 77, 1, 1)), NOW + 10_000)
        self.lib.mav_tick(self.dev, NOW + 10_000)
        replies = self.drain()
        ping = next(m for m in replies if m.get_type() == 'PING')
        self.assertEqual((ping.time_usec, ping.seq), (123456, 77))
        self.assertEqual((ping.target_system, ping.target_component), (250, 1))

        self.feed(encode(lambda m: m.timesync_encode(111, 0)), NOW + 20_000)
        replies = self.drain()
        sync = next(m for m in replies if m.get_type() == 'TIMESYNC')
        self.assertEqual(sync.tc1, 111)
        self.assertEqual(sync.ts1, (NOW + 20_000) * 1000)

    def test_esp32_rc_authority_and_telemetry_echo(self):
        self.heartbeat(NOW, channel=1)
        self.configure(NOW + 5_000, channel=1)
        self.arm(NOW + 10_000, channel=1)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        self.assertEqual(self.state(), VL_WIFI_ACTIVE)
        self.assertEqual(self.lib.mav_source(self.dev), 1)

        channels = [1500, 1750, 1000, 1250] + [0] * 14
        self.feed(encode(lambda m: m.rc_channels_encode(0, 4, *channels, 0)), NOW + 50_000, channel=1)
        self.lib.mav_tick(self.dev, NOW + 50_000)
        body = self.body()
        self.assertAlmostEqual(body[0], .5, places=5)
        self.assertAlmostEqual(body[1], 0., places=5)
        self.assertAlmostEqual(body[2], 1., places=5)  # CH4 left is CCW-positive here
        self.lib.mav_tick(self.dev, NOW + 150_000)  # next 100 ms RC telemetry tick
        echo = [m for m in self.drain(1) if m.get_type() == 'RC_CHANNELS'][-1]
        self.assertEqual((echo.chancount, echo.chan2_raw, echo.chan4_raw), (4, 1750, 1250))

        # The Pi cannot steer while the ESP32 owns authority.
        self.heartbeat(NOW + 160_000, channel=0)
        self.setpoint(NOW + 160_000, -1., 0., 0., channel=0)
        self.lib.mav_tick(self.dev, NOW + 160_000)
        self.assertAlmostEqual(self.body()[0], .5, places=5)

        self.lib.mav_tick(self.dev, NOW + 3_000_000)
        self.assertFalse(self.lib.mav_alive(self.dev, 1))
        self.assertEqual(self.state(), VL_FAILSAFE)

    def test_manual_control_from_pi_scales_to_body(self):
        self.heartbeat(NOW)
        self.configure(NOW + 5_000)
        self.arm(NOW + 10_000)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        self.feed(encode(lambda m: m.manual_control_encode(1, 500, -500, 0, 1000, 0)), NOW + 50_000)
        self.lib.mav_tick(self.dev, NOW + 50_000)
        body = self.body()
        self.assertAlmostEqual(body[0], .5, places=4)
        self.assertAlmostEqual(body[1], -.5, places=4)
        self.assertAlmostEqual(body[2], -2., places=4)

    def test_shutdown_command_is_pi_only_disarmed_and_latches(self):
        self.heartbeat(NOW, channel=1)
        self.drain(1)
        # The ESP32 bridge is not the companion and cannot shut the Pi down.
        self.shutdown(NOW + 1_000, channel=1)
        ack = next(m for m in self.drain(1) if m.get_type() == 'COMMAND_ACK')
        self.assertEqual(ack.command, mav.MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN)
        self.assertEqual(ack.result, mav.MAV_RESULT_DENIED)
        self.assertEqual(self.lib.mav_power_inhibit(self.dev), 0)
        self.assertEqual(self.lib.mav_shutdown_requested(self.dev), 0)

        self.heartbeat(NOW + 2_000)
        # Neither an autopilot reboot nor a companion reboot is a shutdown.
        for param1, param2 in ((1., 2.), (0., 1.)):
            self.shutdown(NOW + 3_000, param1=param1, param2=param2)
            ack = next(m for m in self.drain() if m.get_type() == 'COMMAND_ACK')
            self.assertEqual(ack.result, mav.MAV_RESULT_DENIED)
        self.assertEqual(self.lib.mav_power_inhibit(self.dev), 0)

        self.shutdown(NOW + 4_000)
        ack = next(m for m in self.drain() if m.get_type() == 'COMMAND_ACK')
        self.assertEqual(ack.result, mav.MAV_RESULT_ACCEPTED)
        self.assertEqual(self.lib.mav_power_inhibit(self.dev), 1)
        self.assertEqual(self.lib.mav_shutdown_requested(self.dev), 1)

        # The inhibit blocks every new grant; repeat shutdown ACKs are idempotent.
        self.arm(NOW + 5_000)
        self.assertEqual(self.state(), VL_DISARMED)
        self.drain()  # Consume ARM denial before inspecting the retry ACK.
        self.shutdown(NOW + 6_000)
        ack = next(m for m in self.drain() if m.get_type() == 'COMMAND_ACK')
        self.assertEqual(ack.result, mav.MAV_RESULT_ACCEPTED)

    def test_shutdown_refused_while_armed(self):
        self.heartbeat(NOW)
        self.configure(NOW + 5_000)
        self.arm(NOW + 10_000)
        self.lib.mav_tick(self.dev, NOW + 40_000)
        self.assertEqual(self.state(), VL_PI_ACTIVE)
        self.drain()
        self.shutdown(NOW + 50_000)
        ack = next(m for m in self.drain() if m.get_type() == 'COMMAND_ACK')
        self.assertEqual(ack.result, mav.MAV_RESULT_DENIED)
        self.assertEqual(self.lib.mav_power_inhibit(self.dev), 0)
        self.assertEqual(self.lib.mav_shutdown_requested(self.dev), 0)

    def test_named_health_and_runtime_evidence(self):
        self.heartbeat(NOW)
        self.drain()
        currents = (C.c_float * 4)(.1, .2, .3, .4)
        self.lib.mav_set_health(self.dev, currents, 31.5, 1, .55, 1)
        self.lib.mav_tick(self.dev, NOW + 1_000_000)
        messages = self.drain()
        named = {m.name: m.value for m in messages if m.get_type() == 'NAMED_VALUE_FLOAT'}
        for name, value in (('i_fl', .1), ('i_fr', .2), ('i_rl', .3), ('i_rr', .4),
                            ('ntc_c', 31.5), ('soc', .55), ('stall', 1),
                            ('stk_safe', 111), ('stk_ctrl', 222), ('jit_max', 9),
                            ('cpu_pct', 42), ('cfg_seq', 3), ('cfg_err', 1)):
            self.assertAlmostEqual(named[name], value, places=5)
        battery = next(m for m in messages if m.get_type() == 'BATTERY_STATUS')
        self.assertEqual(battery.battery_remaining, 55)

    def test_fusion_converges_to_level_roll_and_yaw(self):
        dev = self.dev
        self.lib.mav_fusion_reset(dev)
        zero = (C.c_float * 3)(0., 0., 0.)
        level = (C.c_float * 3)(0., 0., 9.81)
        for _ in range(1000):
            self.lib.mav_fusion_update(dev, level, zero, .001)
        e = (C.c_float * 3)()
        self.lib.mav_fusion_euler(dev, e)
        self.assertAlmostEqual(e[0], 0., places=2)
        self.assertAlmostEqual(e[1], 0., places=2)
        self.assertAlmostEqual(e[2], 0., places=2)

        self.lib.mav_fusion_reset(dev)
        right = (C.c_float * 3)(0., 9.81, 0.)
        for _ in range(4000):
            self.lib.mav_fusion_update(dev, right, zero, .001)
        self.lib.mav_fusion_euler(dev, e)
        self.assertAlmostEqual(e[0], math.pi / 2, places=2)

        self.lib.mav_fusion_reset(dev)
        rate = (C.c_float * 3)(0., 0., 1.)
        for _ in range(1000):
            self.lib.mav_fusion_update(dev, level, rate, .001)
        yaw = self.lib.mav_fusion_yaw_ned(dev)
        self.assertAlmostEqual(yaw, 1., delta=.03)  # clockwise-positive NED heading

    def test_imu_payload_feeds_estimator(self):
        # The sensor is mounted z-up, so a level reading is +9.81 on sensor z.
        # The adapter maps it to FRD (-9.81) and feeds gravity (+9.81) to AHRS.
        self.heartbeat(NOW)
        for i in range(200):
            t = NOW + i * 5_000
            self.assertTrue(self.lib.mav_sensor_imu(self.dev, 0., 0., 9.81, 0., 0., 0., t))
            self.lib.mav_tick(self.dev, t + 1_000)
            if i % 20 == 0:
                self.drain()
        imu = next(m for m in self.drain() if m.get_type() == 'HIGHRES_IMU')
        self.assertAlmostEqual(imu.zacc, -9.81, places=2)
        self.assertAlmostEqual(imu.temperature, 25., places=2)




@unittest.skipIf(mav is None, 'pymavlink is not installed')
class LocalSbusTests(unittest.TestCase):
    CFLAGS = ['-DVM_SBUS_AUTHORITY=1']
    setUpClass = classmethod(MavlinkTests.setUpClass.__func__)
    tearDownClass = classmethod(MavlinkTests.tearDownClass.__func__)
    setUp = MavlinkTests.setUp
    tearDown = MavlinkTests.tearDown
    feed = MavlinkTests.feed
    param_set = MavlinkTests.param_set
    configure = MavlinkTests.configure
    state = MavlinkTests.state

    def sbus(self, sampled, now, arm, failsafe=0):
        self.lib.mav_sbus(self.dev, sampled, now, arm, failsafe)
        self.lib.mav_tick(self.dev, now)

    def test_high_at_boot_cannot_arm(self):
        self.configure(NOW)
        self.sbus(NOW + 1000, NOW + 1000, 1)
        self.assertEqual(self.state(), VL_DISARMED)

    def test_held_high_cannot_rearm_after_disarm(self):
        self.configure(NOW)
        self.sbus(NOW + 1000, NOW + 1000, 0)
        self.sbus(NOW + 2000, NOW + 2000, 1)
        self.assertEqual(self.state(), VL_ARMING)
        self.lib.mav_disarm(self.dev, NOW + 3000)
        self.sbus(NOW + 4000, NOW + 4000, 1)
        self.assertEqual(self.state(), VL_DISARMED)
        self.sbus(NOW + 5000, NOW + 5000, 0)
        self.sbus(NOW + 6000, NOW + 6000, 1)
        self.assertEqual(self.state(), VL_ARMING)

    def test_cached_frame_cannot_extend_physical_lease(self):
        self.configure(NOW)
        self.sbus(NOW + 1000, NOW + 1000, 0)
        sampled = NOW + 2000
        self.sbus(sampled, sampled, 1)
        self.sbus(sampled, sampled + 50000, 1)
        self.sbus(sampled, sampled + 99000, 1)
        self.lib.mav_tick(self.dev, sampled + 250000)
        self.assertEqual(self.state(), VL_FAILSAFE)

    def test_failsafe_frame_revokes_rc_immediately(self):
        self.configure(NOW)
        self.sbus(NOW + 1000, NOW + 1000, 0)
        self.sbus(NOW + 2000, NOW + 2000, 1)
        self.sbus(NOW + 3000, NOW + 3000, 1, 1)
        self.assertEqual(self.state(), VL_FAILSAFE)


if __name__ == '__main__':
    unittest.main()
