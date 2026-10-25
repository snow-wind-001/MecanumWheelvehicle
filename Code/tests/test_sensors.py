"""Sensor binary vectors, C parser interoperability and LD06 scan boundaries.

Example: python3 -m unittest discover -s Code/tests -p test_sensors.py -v
"""
import ctypes as C
import math
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Code/ros2_ws/src/mecanum_bridge'))
from mecanum_bridge import protocol as p
from mecanum_bridge.lidar import Assembler, crc8, decode_raw
import test_vehicle as native_tests


def ld06(index, distance=1000, device_ms=None):
    raw = bytearray(47)
    raw[:2] = b'\x54\x2c'
    start = index * 960 % 36000
    struct.pack_into('<HH', raw, 2, 3600, start)
    for point in range(12):
        struct.pack_into('<HB', raw, 6 + 3 * point, distance, 100)
    struct.pack_into('<HH', raw, 42, (start + 880) % 36000,
                     (index * 2667 // 1000) % 30000 if device_ms is None else device_ms)
    raw[-1] = crc8(raw[:-1])
    return bytes(raw)


def ubx_nav_pvt(valid=True):
    payload = bytearray(92)
    struct.pack_into('<H', payload, 4, 2024)
    payload[6:12] = bytes((1, 1, 0, 0, 0, 7))
    payload[20], payload[21], payload[23] = 3, int(valid), 12
    struct.pack_into('<iii', payload, 24, 1210000000, 310000000, 12345)
    struct.pack_into('<II', payload, 40, 1000, 2000)
    struct.pack_into('<I', payload, 68, 100)
    wire = bytearray(b'\xb5\x62\x01\x07') + struct.pack('<H', len(payload)) + payload
    a = b = 0
    for byte in wire[2:]:
        a = (a + byte) & 255
        b = (b + a) & 255
    return bytes(wire + bytes((a, b)))


def sbus_frame(channels=None, flags=0, footer=0):
    channels = [1024] * 16 if channels is None else channels
    packed = sum(value << (11 * index) for index, value in enumerate(channels))
    return b'\x0f' + packed.to_bytes(22, 'little') + bytes((flags, footer))


class LidarTests(unittest.TestCase):
    def test_vendor_crc_basis_and_crc_rejection(self):
        self.assertEqual(crc8(b'\x01'), 0x4d)
        self.assertEqual(decode_raw(ld06(0))[0], 3600)
        damaged = bytearray(ld06(0))
        damaged[9] ^= 1
        with self.assertRaises(ValueError):
            decode_raw(damaged)

    def test_only_full_new_revolutions_publish(self):
        assembler = Assembler()
        scans = []
        for index in range(80):
            scans += assembler.feed_batch(b'\x01' + struct.pack('<Q', 1000000 + index * 2667) + ld06(index))
        self.assertEqual(len(scans), 1)
        self.assertEqual(len(scans[0].ranges), 450)
        self.assertTrue(all(value == 1. for value in scans[0].ranges))
        self.assertAlmostEqual(scans[0].scan_time, .1, places=3)

    def test_lost_revolution_does_not_mix_stale_obstacles(self):
        assembler = Assembler()
        scans = []
        for index in range(170):
            if 45 <= index < 90:
                continue
            scans += assembler.feed_batch(b'\x01' + struct.pack('<Q', 1000000 + index * 2667)
                                          + ld06(index, 2000 if index < 45 else 1000))
        self.assertGreaterEqual(assembler.resets, 1)
        self.assertTrue(scans)
        self.assertTrue(all(math.isnan(value) or value == 1. for scan in scans for value in scan.ranges))

    def test_device_clock_wrap_and_reset(self):
        assembler = Assembler()
        assembler.feed_batch(b'\x01' + struct.pack('<Q', 1000000) + ld06(0, device_ms=29998))
        assembler.feed_batch(b'\x01' + struct.pack('<Q', 1003000) + ld06(1, device_ms=1))
        self.assertEqual(assembler.device_extended_ms, 30001)
        self.assertEqual(assembler.resets, 0)
        assembler.feed_batch(b'\x01' + struct.pack('<Q', 1006000) + ld06(2, device_ms=20000))
        self.assertEqual(assembler.resets, 1)

    def test_missing_points_remain_unknown(self):
        assembler = Assembler()
        scans = []
        for index in range(80):
            if index == 50:
                continue
            scans += assembler.feed_batch(b'\x01' + struct.pack('<Q', 1000000 + index * 2667) + ld06(index))
        self.assertEqual(len(scans), 1)
        self.assertTrue(any(math.isnan(value) for value in scans[0].ranges))
        self.assertFalse(any(math.isinf(value) for value in scans[0].ranges))


class NativeSensorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        native_tests.FirmwareInteropTests.setUpClass()
        cls.lib = native_tests.FirmwareInteropTests.lib
        cls.lib.fixture_sensor_feed.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_size_t, C.c_uint64]
        cls.lib.fixture_sbus_report.argtypes = [C.c_void_p, C.c_uint64, C.c_int]
        cls.lib.fixture_lidar_power.argtypes = [C.c_void_p, C.c_uint32, C.c_int, C.c_int, C.c_int]

    @classmethod
    def tearDownClass(cls):
        native_tests.FirmwareInteropTests.tearDownClass()

    def setUp(self):
        self.device = self.lib.fixture_create()
        wire = p.Frame(p.HELLO, 7, 1, 100).encode()
        self.lib.fixture_feed(self.device, wire, len(wire), 100)
        self.read()

    def tearDown(self):
        self.lib.fixture_destroy(self.device)

    def read(self):
        buffer = C.create_string_buffer(32768)
        n = self.lib.fixture_read(self.device, buffer)
        return p.Parser().feed(buffer.raw[:n])

    def test_sbus_all_channels_flags_and_footer_variants(self):
        channels = [0, 2047, 1, 1024, 172, 1811, 15, 255, 256, 511, 512, 1023, 1025, 2046, 42, 1337]
        for sequence, footer in enumerate((0, 4, 0x14, 0x24, 0x34), 1):
            raw = sbus_frame(channels, flags=15, footer=footer)
            for index in range(0, 25, 2):
                part = raw[index:index + 2]
                self.lib.fixture_sensor_feed(self.device, 3, part, len(part), 1000)
            frame, = self.read()
            self.assertEqual(frame.kind, p.SBUS)
            fields = struct.unpack('<Q16HII', frame.payload)
            self.assertEqual(fields[0], 1000)
            self.assertEqual(list(fields[1:17]), channels)
            self.assertEqual(fields[17], sequence)
            self.assertEqual(fields[18], 0x30f)

    def test_lidar_power_requires_stable_aux_and_rx_and_handles_tick_wrap(self):
        step = lambda ms, pg, rx, fault=0: self.lib.fixture_lidar_power(self.device, ms, pg, rx, fault)
        self.assertEqual(step(0, 0, 1), 0)
        self.assertEqual(step(100, 1, 0), 0)
        self.assertEqual(step(1000, 1, 1), 0)
        self.assertEqual(step(1049, 1, 1), 0)
        self.assertEqual(step(1050, 1, 1), 1)
        self.assertEqual(step(1051, 0, 1), 0)
        self.assertEqual(step(1100, 1, 1), 0)
        self.assertEqual(step(1150, 1, 1), 1)
        self.assertEqual(step(1151, 1, 0), 0)
        self.assertEqual(step(0xffffffe0, 1, 1), 0)
        self.assertEqual(step(0x11, 1, 1), 0)
        self.assertEqual(step(0x12, 1, 1), 1)
        self.assertEqual(step(0x13, 1, 1, 1), 0)
        self.assertEqual(step(0x14, 0, 0), 0)
        self.assertEqual(step(0x1000, 1, 1), 0)
        self.assertEqual(step(0x2000, 1, 1), 0)

    def test_sbus_timeout_transport_error_and_resynchronization(self):
        self.lib.fixture_sbus_report(self.device, 1000, 0)
        self.assertEqual(struct.unpack_from('<I', self.read()[0].payload, 44)[0], 0)
        raw = sbus_frame([0] * 16)
        partial = raw[:10]
        self.lib.fixture_sensor_feed(self.device, 3, partial, len(partial), 2000)
        # A delayed suffix cannot be accepted as a new sample.
        tail = raw[10:]
        self.lib.fixture_sensor_feed(self.device, 3, tail, len(tail), 12000)
        self.assertEqual(self.read(), [])
        self.lib.fixture_sbus_report(self.device, 14000, 1)
        self.assertTrue(struct.unpack_from('<I', self.read()[0].payload, 44)[0] & 16)
        self.lib.fixture_sensor_feed(self.device, 3, raw, len(raw), 15000)
        frame, = self.read()
        self.assertEqual(struct.unpack_from('<I', frame.payload, 44)[0], 3)
        self.lib.fixture_sbus_report(self.device, 114999, 0)
        self.assertEqual(struct.unpack_from('<I', self.read()[0].payload, 44)[0], 3)
        self.lib.fixture_sbus_report(self.device, 115000, 0)
        self.assertEqual(struct.unpack_from('<I', self.read()[0].payload, 44)[0], 1)

    def test_sbus_rejects_bad_flags_footer_and_recovers_nested_header(self):
        raw = sbus_frame([2047] * 16)
        for bad in (sbus_frame(flags=0x80), sbus_frame(footer=0xff)):
            self.lib.fixture_sensor_feed(self.device, 3, bad, len(bad), 1000)
            self.assertEqual(self.read(), [])
        stream = bytes((15, 255, 255, 255, 255)) + raw
        self.lib.fixture_sensor_feed(self.device, 3, stream, len(stream), 2000)
        frame, = self.read()
        self.assertEqual(struct.unpack('<Q16HII', frame.payload)[1:17], (2047,) * 16)

    def test_gnss_raw_uart_to_wire_and_invalid_fix(self):
        wire = ubx_nav_pvt()
        for index in range(0, len(wire), 3):
            part = wire[index:index + 3]
            self.lib.fixture_sensor_feed(self.device, 0, part, len(part), 1234567)
        frame, = self.read()
        self.assertEqual(frame.kind, p.GNSS)
        self.assertEqual(struct.unpack_from('<iii', frame.payload, 16), (310000000, 1210000000, 12345))
        self.assertEqual(struct.unpack_from('<Q', frame.payload, 8)[0], 1704067200000)
        self.assertEqual(struct.unpack_from('<I', frame.payload, 52)[0], 3)
        wire = ubx_nav_pvt(False)
        self.lib.fixture_sensor_feed(self.device, 0, wire, len(wire), 1234568)
        frame, = self.read()
        self.assertEqual(struct.unpack_from('<I', frame.payload, 52)[0] & 1, 0)
        wire = bytearray(ubx_nav_pvt())
        wire[30] ^= 1
        self.lib.fixture_sensor_feed(self.device, 0, bytes(wire), len(wire), 1234569)
        self.assertEqual(self.read(), [])

    def test_imu_raw_register_scaling(self):
        raw = struct.pack('>7h', 0, 0, 0, 8192, 6550, 0, 0)
        self.lib.fixture_sensor_feed(self.device, 2, raw, len(raw), 1234567)
        frame, = self.read()
        self.assertEqual(frame.kind, p.IMU)
        self.assertEqual(struct.unpack_from('<i', frame.payload, 16)[0], 9806650)
        self.assertAlmostEqual(struct.unpack_from('<i', frame.payload, 20)[0] * 1e-6, math.radians(100), places=6)

    def test_continuous_lidar_stream_recovers_after_truncation(self):
        wire = ld06(0)[:10] + b''.join(ld06(index) for index in range(1, 5))
        for byte in wire:
            self.lib.fixture_sensor_feed(self.device, 1, bytes((byte,)), 1, 1234567)
        frames = self.read()
        self.assertEqual(len(frames), 4)
        for frame in frames:
            self.assertEqual(frame.kind, p.LIDAR)
            decode_raw(frame.payload[9:])


if __name__ == '__main__':
    unittest.main()
