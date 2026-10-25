"""Exercise the Python host against compiled production STM32 protocol/state code.

Example: python3 -m unittest discover -s Code/tests -v
"""
import ctypes as C
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Code/ros2_ws/src/mecanum_bridge'))
from mecanum_bridge import protocol as p
from mecanum_bridge.client import Client, calibration_payload
from mecanum_bridge.odometry import Odometry

CALIBRATION = dict(radius_m=.05, half_length_m=.15, half_width_m=.12, counts_per_rev=2048,
                   encoder_sign=[1, 1, 1, 1], motor_sign=[1, 1, 1, 1],
                   max_wheel_rad_s=20., max_wheel_accel_rad_s2=100., kp=.1, ki=.2,
                   feedforward=.02, duty_limit=.5, speed_filter_s=.01, reversal_rad_s=.5)


class CodecTests(unittest.TestCase):
    def test_odometry_forward_lateral_turn_and_old_sample(self):
        import math
        odom = Odometry(.05, .15, .12, 2048)
        odom.update([0, 0, 0, 0], 1)
        odom.update([2048] * 4, 1000001)
        self.assertAlmostEqual(odom.x, .1 * math.pi)
        self.assertAlmostEqual(odom.y, 0)
        odom.update([0, 4096, 4096, 0], 2000001)
        self.assertAlmostEqual(odom.y, .1 * math.pi)
        self.assertFalse(odom.update([9999] * 4, 1))
        self.assertAlmostEqual(odom.y, .1 * math.pi)

    def test_crc_vector_and_all_lengths_fragmented(self):
        self.assertEqual(p.crc16(b'123456789'), 0x29b1)
        randomizer = random.Random(42)
        for length in range(257):
            frame = p.Frame(0x11, 0xfedcba98, 0xffffffff, 0xfedcba9876543210,
                            bytes(randomizer.randrange(256) for _ in range(length)))
            wire = frame.encode()
            self.assertLessEqual(len(wire), p.MAX_WIRE)
            parser = p.Parser()
            decoded = []
            for byte in wire:
                decoded += parser.feed(bytes([byte]))
            self.assertEqual(decoded, [frame])

    def test_recovery_after_oversize_crc_and_truncation(self):
        frame = p.Frame(1, 9, 100, 500)
        wire = frame.encode()
        damaged = bytearray(wire)
        damaged[3] ^= 0x40
        parser = p.Parser()
        self.assertEqual(parser.feed(b'\x01' * 10000 + b'\0' + damaged + wire[:7] + b'\0' + wire), [frame])
        self.assertEqual(parser.rejected, 3)
        self.assertLessEqual(len(parser.buffer), 282)

    def test_calibration_requires_real_counts_and_valid_signs(self):
        self.assertEqual(len(calibration_payload(CALIBRATION)), 64)
        for override in ({'counts_per_rev': 0}, {'encoder_sign': [1, 1, 0, 1]}, {'radius_m': float('nan')}):
            with self.assertRaises(ValueError):
                calibration_payload(CALIBRATION | override)


class FirmwareInteropTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        app = ROOT / 'Code/MecanumB_Control/App'
        sources = [str(app / 'src' / (name + '.c')) for name in ('vehicle_protocol', 'vehicle_control', 'vehicle_link', 'vehicle_sensors', 'vehicle_tx', 'vehicle_power')]
        target = str(Path(cls.temp.name) / 'firmware.so')
        subprocess.run(['gcc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(app / 'include'), str(ROOT / 'Code/tests/native_fixture.c'),
                        *sources, '-lm', '-o', target], check=True, capture_output=True)
        cls.lib = C.CDLL(target)
        cls.lib.fixture_create.restype = C.c_void_p
        cls.lib.fixture_destroy.argtypes = [C.c_void_p]
        cls.lib.fixture_pi_inputs.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int]
        cls.lib.fixture_tick.argtypes = [C.c_void_p, C.c_uint64, C.c_uint32, C.c_int]
        cls.lib.fixture_feed.argtypes = [C.c_void_p, C.c_char_p, C.c_size_t, C.c_uint64]
        cls.lib.fixture_read.argtypes = [C.c_void_p, C.c_void_p]
        cls.lib.fixture_read.restype = C.c_size_t
        cls.lib.fixture_state.argtypes = [C.c_void_p]
        cls.lib.fixture_state.restype = C.c_uint
        cls.lib.fixture_permitted.argtypes = [C.c_void_p, C.c_uint64]
        cls.lib.fixture_status.argtypes = [C.c_void_p, C.c_uint64]
        cls.lib.fixture_power_step.argtypes = [C.c_void_p, C.c_uint32, C.c_int, C.c_int, C.c_int, C.c_int, C.c_int]
        cls.lib.fixture_power_step.restype = C.c_uint
        cls.lib.fixture_power_report.argtypes = [C.c_void_p, C.c_uint64]
        cls.lib.fixture_power_applied.argtypes = [C.c_void_p, C.c_uint32, C.c_int, C.c_int]
        cls.lib.fixture_mavlink.argtypes = [C.c_void_p]
        cls.lib.fixture_rc_frame.argtypes = [C.c_void_p, C.c_uint64]
        cls.lib.fixture_source.argtypes = [C.c_void_p]
        cls.lib.fixture_source.restype = C.c_uint
        cls.lib.fixture_faults.argtypes = [C.c_void_p]
        cls.lib.fixture_faults.restype = C.c_uint
        cls.lib.fixture_mav_arm.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_uint64]
        cls.lib.fixture_mav_motion.argtypes = [C.c_void_p, C.c_uint, C.c_float, C.c_float,
                                               C.c_float, C.c_uint32, C.c_uint64]
        cls.lib.fixture_body.argtypes = [C.c_void_p, C.POINTER(C.c_float)]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.device = self.lib.fixture_create()
        self.assertTrue(self.device)
        self.lib.fixture_pi_inputs(self.device, 1, 1, 0)
        self.outgoing, self.events = [], []
        self.client = Client(self.outgoing.append, lambda *event: self.events.append(event))
        self.now = 1.
        self.client.tick(self.now)
        self.pump()
        self.assertTrue(self.client.connected)

    def tearDown(self):
        self.lib.fixture_destroy(self.device)

    def pump(self, status=False):
        for wire in self.outgoing:
            # Exercise arbitrary DMA/serial fragmentation at the MCU parser.
            for i in range(0, len(wire), 7):
                part = wire[i:i + 7]
                self.lib.fixture_feed(self.device, part, len(part), int(self.now * 1e6))
        self.outgoing.clear()
        self.lib.fixture_tick(self.device, int(self.now * 1e6), 0, 1)
        if status:
            self.lib.fixture_status(self.device, int(self.now * 1e6))
        data = C.create_string_buffer(32768)
        n = self.lib.fixture_read(self.device, data)
        for i in range(0, n, 11):
            self.client.feed(data.raw[i:min(i + 11, n)], self.now)

    def configure_and_arm(self):
        self.client.configure(CALIBRATION, self.now)
        self.pump()
        self.assertTrue(self.client.configured)
        self.client.arm(True, self.now)
        self.pump()
        for _ in range(12):
            self.now += .01
            self.client.command_velocity(.1, .05, .1, self.now)
            self.client.tick(self.now)
            self.pump(status=True)
        self.assertTrue(self.client.armed)
        self.assertTrue(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))

    def test_pi_inputs_required_before_arm(self):
        self.lib.fixture_pi_inputs(self.device, 1, 0, 0)
        self.client.configure(CALIBRATION, self.now)
        self.pump()
        self.client.arm(True, self.now)
        self.pump()
        self.assertFalse(self.client.armed)
        self.assertEqual(self.lib.fixture_state(self.device), 1)
        self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))
        self.assertTrue(any(e[0] == 'arm' and not e[1] for e in self.events))

    def test_pi_ready_power_and_halted_revoke_authorization(self):
        for inputs in ((0, 1, 0), (1, 0, 0), (1, 1, 1)):
            with self.subTest(inputs=inputs):
                self.lib.fixture_pi_inputs(self.device, 1, 1, 0)
                self.client.arm(False, self.now)
                self.pump(status=True)
                self.configure_and_arm()
                self.lib.fixture_pi_inputs(self.device, *inputs)
                self.assertEqual(self.lib.fixture_state(self.device), 6)
                self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))
                self.lib.fixture_pi_inputs(self.device, 1, 1, 0)
                self.assertEqual(self.lib.fixture_state(self.device), 6)
                self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))
                self.pump(status=True)
                self.assertFalse(self.client.armed)

    def test_pi_input_loss_revokes_pending_arm_token(self):
        self.client.configure(CALIBRATION, self.now)
        self.pump()
        self.client.arm(True, self.now)
        self.pump()  # Lease delivered; initial zero MOTION is still in host queue.
        self.lib.fixture_pi_inputs(self.device, 1, 0, 0)
        self.lib.fixture_pi_inputs(self.device, 1, 1, 0)
        self.pump(status=True)
        self.assertEqual(self.lib.fixture_state(self.device), 6)
        self.assertFalse(self.client.armed)
        self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))

    def test_shutdown_rejected_while_active_and_inhibits_all_new_grants(self):
        self.configure_and_arm()
        with self.assertRaises(ValueError):
            self.client.shutdown_pi(self.now)
        # Bypass the host guard and check the MCU independently.
        self.client._send(p.POWER, self.now, bytes((1, 1, 0, 0)), 'shutdown_pi')
        self.pump()
        self.assertTrue(any(e[0] == 'shutdown_pi' and not e[1] for e in self.events))
        self.client.arm(False, self.now)
        self.pump(status=True)
        self.client.shutdown_pi(self.now)
        self.pump()
        self.assertTrue(any(e[0] == 'shutdown_pi' and e[1] and 'waiting' in e[2] for e in self.events))
        # Acceptance alone cannot assert the latch-clear output.
        result = self.lib.fixture_power_step(self.device, 1000, 0, 1, 0, 1, 1)
        self.assertEqual(result & 255, 1)
        self.assertFalse(result & 0x20000)
        self.client.arm(True, self.now)
        self.pump()
        self.assertFalse(self.client.armed)
        self.assertTrue(any(e[0] == 'arm' and not e[1] for e in self.events))
        self.client.clear_fault(self.now)
        self.pump()
        self.client.arm(True, self.now)
        self.pump()
        self.assertFalse(self.client.armed)

    def test_rc_source_arms_steers_and_expires_after_250ms(self):
        self.lib.fixture_mavlink(self.device)
        self.client.configure(CALIBRATION, self.now)
        self.pump()
        now = int(self.now * 1e6)
        # Arming without a frame is expired, not accepted.
        self.assertEqual(self.lib.fixture_mav_arm(self.device, 2, 1, now), 5)
        self.lib.fixture_rc_frame(self.device, now)
        self.assertEqual(self.lib.fixture_mav_arm(self.device, 2, 1, now), 0)
        self.assertEqual(self.lib.fixture_state(self.device), 2)
        self.lib.fixture_tick(self.device, now + 20_000, 0, 1)
        self.assertEqual(self.lib.fixture_state(self.device), 3)  # VL_RC_ACTIVE
        self.assertEqual(self.lib.fixture_source(self.device), 2)
        self.assertEqual(self.lib.fixture_mav_motion(self.device, 2, .2, -.1, .3, 250, now + 20_000), 0)
        body = (C.c_float * 3)()
        self.lib.fixture_body(self.device, body)
        self.assertAlmostEqual(body[0], .2, places=5)
        self.assertAlmostEqual(body[1], -.1, places=5)
        self.assertAlmostEqual(body[2], .3, places=5)
        self.assertTrue(self.lib.fixture_permitted(self.device, now + 20_000))

        # No further SBUS frame: authority expires 250 ms after the last one,
        # even though the vehicle is otherwise healthy.
        self.lib.fixture_tick(self.device, now + 270_000, 0, 1)
        self.assertEqual(self.lib.fixture_state(self.device), 6)  # VL_FAILSAFE
        self.assertFalse(self.lib.fixture_permitted(self.device, now + 270_000))

    def test_full_session_arm_motion_renewal_and_explicit_stop(self):
        self.configure_and_arm()
        old_token = self.client.token
        for _ in range(110):
            self.now += .01
            self.client.command_velocity(.1, -.1, .2, self.now)
            self.client.tick(self.now)
            self.pump(status=True)
        self.assertNotEqual(self.client.token, old_token)
        self.assertTrue(self.client.armed)
        self.client.arm(False, self.now)
        self.pump(status=True)
        self.assertEqual(self.lib.fixture_state(self.device), 1)
        self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))

    def test_host_cmd_vel_timeout_requires_explicit_rearm(self):
        self.configure_and_arm()
        for _ in range(30):
            self.now += .01
            self.client.tick(self.now)
            self.pump(status=True)
        self.assertFalse(self.client.armed)
        self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))
        self.client.command_velocity(.2, 0, 0, self.now)
        self.client.tick(self.now)
        self.pump(status=True)
        self.assertFalse(self.client.armed)

    def test_mcu_enforces_expiry_without_host_tick(self):
        self.configure_and_arm()
        self.now += .3
        self.pump(status=True)
        self.assertEqual(self.lib.fixture_state(self.device), 6)
        self.assertFalse(self.client.armed)

    def test_late_motion_cannot_revive_command_before_safety_tick(self):
        self.configure_and_arm()
        import struct
        # No MCU tick occurs between expiry and reception of this valid-token packet.
        self.now += .3
        seq = (self.client.seq + 1) & 0xffffffff
        payload = struct.pack('<IIHBBiii', self.client.token, self.client.epoch, 100, 5, 0, 100, 0, 0)
        wire = p.Frame(p.MOTION, self.client.session, seq, int(self.now * 1e6), payload).encode()
        self.lib.fixture_feed(self.device, wire, len(wire), int(self.now * 1e6))
        self.assertEqual(self.lib.fixture_state(self.device), 6)
        self.assertFalse(self.lib.fixture_permitted(self.device, int(self.now * 1e6)))

    def test_reboot_rehandshakes_without_rearming_or_reusing_calibration(self):
        self.configure_and_arm()
        session = self.client.session
        self.lib.fixture_destroy(self.device)
        self.device = self.lib.fixture_create()
        # The restarted MCU's response sequence begins below the previous boot.
        for _ in range(220):
            self.now += .01
            self.client.command_velocity(.1, 0, 0, self.now)
            self.client.tick(self.now)
            self.pump()
        self.assertNotEqual(self.client.session, session)
        self.assertTrue(self.client.connected)
        self.assertFalse(self.client.armed)
        self.assertFalse(self.client.configured)
        self.assertEqual(self.lib.fixture_state(self.device), 1)


if __name__ == '__main__':
    unittest.main()
