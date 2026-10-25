"""A/B parameter log and electrical-health tests against the production C code.

Example: python3 -m unittest Code.tests.test_storage_health -v
"""
import ctypes as C
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'Code/MecanumB_Control/App'

FAULT_OVERCURRENT = 1 << 3
FAULT_STALL = 1 << 4
FAULT_OVERTEMP = 1 << 5
FAULT_UNDERVOLT = 1 << 6
FAULT_SENSOR = 1 << 7

CRC32_123456789 = 0xCBF43926


class StorageHealthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        sources = [str(APP / 'src' / (name + '.c')) for name in (
            'vehicle_store', 'vehicle_health', 'vehicle_link', 'vehicle_control', 'vehicle_protocol')]
        target = str(Path(cls.temp.name) / 'storage_health_fixture.so')
        subprocess.run(['gcc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        *getattr(cls, 'CFLAGS', []), '-I' + str(APP / 'include'), str(ROOT / 'Code/tests/store_health_fixture.c'),
                        *sources, '-lm', '-o', target], check=True, capture_output=True)
        cls.lib = C.CDLL(target)
        L = cls.lib
        L.sh_create.restype = C.c_void_p
        L.sh_destroy.argtypes = [C.c_void_p]
        L.sh_store_init.argtypes = [C.c_void_p, C.c_int]
        L.sh_store_save.argtypes = [C.c_void_p]
        L.sh_store_seq.argtypes = [C.c_void_p]
        L.sh_store_seq.restype = C.c_uint32
        L.sh_store_errors.argtypes = [C.c_void_p]
        L.sh_store_errors.restype = C.c_uint32
        L.sh_store_writable.argtypes = [C.c_void_p]
        L.sh_store_writable.restype = C.c_uint32
        L.sh_store_active.argtypes = [C.c_void_p]
        L.sh_store_active.restype = C.c_uint32
        L.sh_erase_count.argtypes = [C.c_void_p, C.c_uint]
        L.sh_erase_count.restype = C.c_uint32
        L.sh_config_radius.argtypes = [C.c_void_p]
        L.sh_config_radius.restype = C.c_float
        L.sh_config_set_radius.argtypes = [C.c_void_p, C.c_float]
        L.sh_flash_corrupt.argtypes = [C.c_void_p, C.c_uint, C.c_uint32]
        L.sh_flash_word.argtypes = [C.c_void_p, C.c_uint, C.c_uint32]
        L.sh_flash_word.restype = C.c_uint32
        L.vs_crc32.argtypes = [C.c_void_p, C.c_uint32]
        L.vs_crc32.restype = C.c_uint32

        L.sh_health_init.argtypes = [C.c_void_p]
        L.sh_health_step.argtypes = [C.c_void_p, C.POINTER(C.c_uint16), C.c_int,
                                     C.c_float, C.c_float, C.c_int, C.c_float]
        L.sh_health_step.restype = C.c_uint32
        L.sh_current.argtypes = [C.c_void_p, C.c_uint]
        L.sh_current.restype = C.c_float
        L.sh_vbat.argtypes = [C.c_void_p]
        L.sh_vbat.restype = C.c_float
        L.sh_soc.argtypes = [C.c_void_p]
        L.sh_soc.restype = C.c_float
        L.sh_soc_valid.argtypes = [C.c_void_p]
        L.sh_ntc_valid.argtypes = [C.c_void_p]
        L.sh_stall.argtypes = [C.c_void_p, C.c_uint]
        L.sh_sensor_lost.argtypes = [C.c_void_p]
        L.sh_health_faults.argtypes = [C.c_void_p]
        L.sh_health_faults.restype = C.c_uint32
        L.vh_adc_volts.argtypes = [C.c_uint16]
        L.vh_adc_volts.restype = C.c_float
        L.vh_current_a.argtypes = [C.c_uint16]
        L.vh_current_a.restype = C.c_float
        L.vh_pack_mv.argtypes = [C.c_uint16, C.c_float]
        L.vh_pack_mv.restype = C.c_uint16
        L.vh_soc.argtypes = [C.c_uint16]
        L.vh_soc.restype = C.c_float

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.dev = self.lib.sh_create()
        self.assertTrue(self.dev)

    def tearDown(self):
        self.lib.sh_destroy(self.dev)

    # ---- parameter log -------------------------------------------------

    def test_crc32_vector(self):
        data = b'123456789'
        buf = C.create_string_buffer(data, len(data))
        self.assertEqual(self.lib.vs_crc32(buf, len(data)), CRC32_123456789)

    def test_empty_flash_prepares_slot_zero_and_saves(self):
        self.assertFalse(self.lib.sh_store_init(self.dev, 1))
        self.assertEqual(self.lib.sh_erase_count(self.dev, 0), 0)
        self.assertEqual(self.lib.sh_store_seq(self.dev), 0)
        self.assertTrue(self.lib.sh_store_writable(self.dev))
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertEqual(self.lib.sh_store_seq(self.dev), 1)
        # Little-endian magic "MCV1" at the start of the record.
        self.assertEqual(self.lib.sh_flash_word(self.dev, 0, 0), 0x4D435631)

    def test_newest_valid_record_wins_and_bad_crc_falls_back(self):
        self.lib.sh_store_init(self.dev, 1)
        self.lib.sh_config_set_radius(self.dev, .05)
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.lib.sh_config_set_radius(self.dev, .06)
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertTrue(self.lib.sh_store_init(self.dev, 1))
        self.assertAlmostEqual(self.lib.sh_config_radius(self.dev), .06, places=6)
        self.assertEqual(self.lib.sh_store_seq(self.dev), 2)

        # The second record (offset 84) becomes unreadable; the first one is
        # still a complete, older configuration.
        self.lib.sh_flash_corrupt(self.dev, 0, 84 + 12)
        self.assertTrue(self.lib.sh_store_init(self.dev, 1))
        self.assertAlmostEqual(self.lib.sh_config_radius(self.dev), .05, places=6)
        self.assertEqual(self.lib.sh_store_seq(self.dev), 1)

    def test_full_slot_stops_writing_and_rotates_at_boot(self):
        self.lib.sh_store_init(self.dev, 1)
        records = 4096 // 84
        for _ in range(records):
            self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertFalse(self.lib.sh_store_writable(self.dev))
        self.assertEqual(self.lib.sh_store_seq(self.dev), records)
        self.assertFalse(self.lib.sh_store_save(self.dev))
        self.assertEqual(self.lib.sh_store_errors(self.dev), 1)

        # The next boot erases the older slot and appends there.
        self.assertTrue(self.lib.sh_store_init(self.dev, 1))
        self.assertEqual(self.lib.sh_store_active(self.dev), 1)
        self.assertTrue(self.lib.sh_store_writable(self.dev))
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertEqual(self.lib.sh_store_seq(self.dev), records + 1)

    def test_read_only_boot_without_erase_is_not_writable(self):
        self.assertFalse(self.lib.sh_store_init(self.dev, 0))
        self.assertFalse(self.lib.sh_store_writable(self.dev))
        self.assertFalse(self.lib.sh_store_save(self.dev))

    def test_torn_magic_is_consumed_and_later_record_remains_visible(self):
        self.lib.sh_store_init(self.dev, 1)
        for radius in (.05, .06, .07):
            self.lib.sh_config_set_radius(self.dev, radius)
            self.assertTrue(self.lib.sh_store_save(self.dev))
        self.lib.sh_flash_corrupt(self.dev, 0, 84)
        self.assertTrue(self.lib.sh_store_init(self.dev, 1))
        self.assertAlmostEqual(self.lib.sh_config_radius(self.dev), .07, places=6)
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertEqual(self.lib.sh_store_seq(self.dev), 4)

    def test_torn_tail_is_not_reprogrammed(self):
        self.lib.sh_store_init(self.dev, 1)
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.lib.sh_flash_corrupt(self.dev, 0, 84)
        self.assertTrue(self.lib.sh_store_init(self.dev, 1))
        self.assertTrue(self.lib.sh_store_save(self.dev))
        self.assertEqual(self.lib.sh_flash_word(self.dev, 0, 168), 0x4D435631)

    def test_oc_recovery_requires_continuous_low_current(self):
        high = (C.c_uint16 * 7)(1000, 0, 0, 0, 1500, 1500, 0)
        band = (C.c_uint16 * 7)(550, 0, 0, 0, 1500, 1500, 0)
        low = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, 0)
        for _ in range(20):
            self.lib.sh_health_step(self.dev, high, 1, 0., 0., 0, .001)
        for _ in range(1000):
            self.lib.sh_health_step(self.dev, band, 1, 0., 0., 0, .001)
        self.assertTrue(self.lib.sh_health_step(self.dev, low, 1, 0., 0., 0, .001) & FAULT_OVERCURRENT)

    def test_uv_recovery_requires_continuous_recovered_voltage(self):
        low = (C.c_uint16 * 7)(0, 0, 0, 0, 1200, 1200, 0)
        band = (C.c_uint16 * 7)(0, 0, 0, 0, 1300, 1300, 0)
        high = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, 0)
        for _ in range(1000):
            self.lib.sh_health_step(self.dev, low, 1, 0., 0., 0, .001)
        for _ in range(2000):
            self.lib.sh_health_step(self.dev, band, 1, 0., 0., 0, .001)
        self.assertTrue(self.lib.sh_health_step(self.dev, high, 1, 0., 0., 0, .001) & FAULT_UNDERVOLT)

    # ---- electrical health ---------------------------------------------

    def test_adc_conversions_match_the_divider_and_shunt(self):
        self.assertAlmostEqual(self.lib.vh_adc_volts(4095), 3.3, places=5)
        self.assertAlmostEqual(self.lib.vh_current_a(2048), 3.3, places=2)
        # 120k/10k divider, 12-bit ADC: mid-scale is ~21.455 V.
        self.assertEqual(self.lib.vh_pack_mv(2048, 13.0), 21455)
        self.assertAlmostEqual(self.lib.vh_soc(16800), 1.0, places=5)
        self.assertAlmostEqual(self.lib.vh_soc(13600), 0.0, places=5)
        self.assertAlmostEqual(self.lib.vh_soc(15200), .5, places=5)

    def test_overcurrent_latches_and_clears_with_hysteresis(self):
        adc = (C.c_uint16 * 7)(4095, 0, 0, 0, 2145, 2145, 0)  # ~6.6 A on FL
        for _ in range(20):
            faults = self.lib.sh_health_step(self.dev, adc, 1, 0., 0., 0, .001)
        self.assertTrue(faults & FAULT_OVERCURRENT)
        self.assertAlmostEqual(self.lib.sh_current(self.dev, 0), 6.6, places=2)

        quiet = (C.c_uint16 * 7)(0, 0, 0, 0, 2145, 2145, 0)
        for _ in range(999):
            faults = self.lib.sh_health_step(self.dev, quiet, 1, 0., 0., 0, .001)
        self.assertTrue(faults & FAULT_OVERCURRENT)
        faults = self.lib.sh_health_step(self.dev, quiet, 1, 0., 0., 0, .001)
        self.assertFalse(faults & FAULT_OVERCURRENT)

    def test_reported_one_amp_motor_stall_is_detected_below_hardware_limit(self):
        adc = (C.c_uint16 * 7)(620, 0, 0, 0, 1500, 1500, 0)  # nominal 1 A
        for _ in range(500):
            faults = self.lib.sh_health_step(self.dev, adc, 1, 0., .5, 1, .001)
        self.assertTrue(faults & FAULT_STALL)
        self.assertFalse(faults & FAULT_OVERCURRENT)

    def test_stall_requires_demand_current_and_no_motion(self):
        adc = (C.c_uint16 * 7)(2048, 0, 0, 0, 2145, 2145, 0)  # 3.3 A on FL
        for _ in range(499):
            faults = self.lib.sh_health_step(self.dev, adc, 1, 0., .5, 1, .001)
        self.assertFalse(faults & FAULT_STALL)
        faults = self.lib.sh_health_step(self.dev, adc, 1, 0., .5, 1, .001)
        self.assertTrue(faults & FAULT_STALL)
        self.assertTrue(self.lib.sh_stall(self.dev, 0))

        # Releasing the demand clears the stall after the 2 s hysteresis.
        for _ in range(2000):
            faults = self.lib.sh_health_step(self.dev, adc, 1, 0., 0., 0, .001)
        self.assertFalse(faults & FAULT_STALL)

    def test_undervoltage_latches_and_recovers(self):
        low = (C.c_uint16 * 7)(0, 0, 0, 0, 1200, 1200, 0)  # ~12.6 V pack
        for _ in range(1000):
            faults = self.lib.sh_health_step(self.dev, low, 1, 0., 0., 0, .001)
        self.assertTrue(faults & FAULT_UNDERVOLT)

        high = (C.c_uint16 * 7)(0, 0, 0, 0, 1700, 1700, 0)  # ~17.8 V pack
        for _ in range(2000):
            faults = self.lib.sh_health_step(self.dev, high, 1, 0., 0., 0, .001)
        self.assertFalse(faults & FAULT_UNDERVOLT)

    def test_missing_adc_becomes_a_sensor_fault(self):
        adc = (C.c_uint16 * 7)()
        for _ in range(999):
            faults = self.lib.sh_health_step(self.dev, adc, 0, 0., 0., 0, .001)
        self.assertFalse(faults & FAULT_SENSOR)
        faults = self.lib.sh_health_step(self.dev, adc, 0, 0., 0., 0, .001)
        self.assertTrue(faults & FAULT_SENSOR)
        self.assertTrue(self.lib.sh_sensor_lost(self.dev))

    def test_ntc_is_disabled_until_the_divider_is_verified(self):
        adc = (C.c_uint16 * 7)(0, 0, 0, 0, 2145, 2145, 2048)
        faults = self.lib.sh_health_step(self.dev, adc, 1, 0., 0., 0, .001)
        self.assertFalse(self.lib.sh_ntc_valid(self.dev))
        self.assertFalse(faults & FAULT_OVERTEMP)




class EnabledNtcTests(unittest.TestCase):
    CFLAGS = ['-DVH_NTC_ENABLED=1']
    setUpClass = classmethod(StorageHealthTests.setUpClass.__func__)
    tearDownClass = classmethod(StorageHealthTests.tearDownClass.__func__)
    setUp = StorageHealthTests.setUp
    tearDown = StorageHealthTests.tearDown

    def test_temperature_recovery_does_not_accumulate_in_hysteresis_band(self):
        import math
        def adc_for(celsius):
            resistance = 10000 * math.exp(3950 * (1 / (celsius + 273.15) - 1 / 298.15))
            return round(4095 * resistance / (10000 + resistance))
        hot = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, adc_for(90))
        band = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, adc_for(70))
        cool = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, adc_for(50))
        for _ in range(1000):
            fault = self.lib.sh_health_step(self.dev, hot, 1, 0., 0., 0, .001)
        self.assertTrue(fault & FAULT_OVERTEMP)
        for _ in range(2000):
            self.lib.sh_health_step(self.dev, band, 1, 0., 0., 0, .001)
        self.assertTrue(self.lib.sh_health_step(self.dev, cool, 1, 0., 0., 0, .001) & FAULT_OVERTEMP)
        for _ in range(1999):
            fault = self.lib.sh_health_step(self.dev, cool, 1, 0., 0., 0, .001)
        self.assertFalse(fault & FAULT_OVERTEMP)

    def test_lost_adc_never_reports_a_valid_stale_temperature(self):
        adc = (C.c_uint16 * 7)(0, 0, 0, 0, 1500, 1500, 2048)
        self.lib.sh_health_step(self.dev, adc, 1, 0., 0., 0, .001)
        self.assertTrue(self.lib.sh_ntc_valid(self.dev))
        self.lib.sh_health_step(self.dev, adc, 0, 0., 0., 0, .001)
        self.assertFalse(self.lib.sh_ntc_valid(self.dev))


if __name__ == '__main__':
    unittest.main()
