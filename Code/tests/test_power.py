"""Exercise production Pi power sequencing with simulated physical inputs.

Example: python3 -m unittest discover -s Code/tests -p test_power.py -v
"""
import unittest
import test_vehicle as native


class PowerStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        native.FirmwareInteropTests.setUpClass()
        cls.lib = native.FirmwareInteropTests.lib

    @classmethod
    def tearDownClass(cls):
        native.FirmwareInteropTests.tearDownClass()

    def setUp(self):
        self.device = self.lib.fixture_create()

    def tearDown(self):
        self.lib.fixture_destroy(self.device)

    def step(self, ms, request=0, good=1, halted=0, still=1, off=1):
        value = self.lib.fixture_power_step(self.device, ms & 0xffffffff, request, good, halted, still, off)
        return (value & 255, value >> 8 & 255, bool(value & 0x10000), bool(value & 0x20000), bool(value & 0x40000))

    def test_encoder_activity_and_enabled_driver_delay_shutdown_request(self):
        self.assertEqual(self.step(0, request=1), (1, 0, False, False, True))
        self.step(90, still=0)
        self.step(100)
        self.assertEqual(self.step(199)[0], 1)
        self.step(200, off=0)
        self.step(210)
        self.assertEqual(self.step(309)[0], 1)
        self.assertEqual(self.step(310), (2, 0, True, False, True))

    def test_halted_glitch_and_power_good_fall_during_cut_pulse(self):
        self.step(0, request=1)
        self.step(100)
        self.step(110, halted=1)
        self.assertFalse(self.step(129, halted=1)[3])
        self.step(130)  # A short HALTED pulse is insufficient.
        self.step(140, halted=1)
        self.assertEqual(self.step(160, halted=1), (3, 0, True, True, True))
        self.assertTrue(self.step(179, good=0, halted=1)[3])
        self.assertEqual(self.step(180, good=0)[0], 4)
        self.step(190, good=0)
        self.assertEqual(self.step(210, good=0), (5, 0, False, False, True))
        self.step(800, good=1, halted=1)
        self.assertTrue(self.step(900, good=1, halted=1)[4])  # No unlock while backpowered/halted.
        self.step(910)
        self.assertEqual(self.step(960), (0, 0, False, False, False))

    def test_stop_and_halt_timeouts_never_force_cut(self):
        self.step(0, request=1, still=0)
        self.assertEqual(self.step(5000, still=0), (6, 1, False, False, True))
        self.assertEqual(self.step(99999, halted=1), (6, 1, False, False, True))
        self.lib.fixture_destroy(self.device)
        self.device = self.lib.fixture_create()
        self.step(0, request=1)
        self.step(100)
        self.assertEqual(self.step(60100), (6, 2, True, False, True))
        self.assertFalse(self.step(70000, halted=1)[3])  # Late HALTED cannot retry a timed-out transaction.
        self.assertFalse(self.step(70010, good=0)[2])  # Release PC6 when power is lost.
        self.assertEqual(self.step(80000), (6, 2, False, False, True))

    def test_high_halted_at_boot_and_supply_loss_do_not_cut(self):
        for ms in (0, 50, 500, 1000):
            self.assertEqual(self.step(ms, halted=1), (0, 0, False, False, False))
        self.assertEqual(self.step(1010, request=1, halted=1), (6, 4, False, False, True))
        self.lib.fixture_destroy(self.device)
        self.device = self.lib.fixture_create()
        self.step(0, request=1)
        self.step(100)
        self.assertEqual(self.step(110, good=0), (6, 3, False, False, True))
        self.assertFalse(self.step(500, good=1, halted=1)[3])

    def test_local_kernel_poweroff_after_observed_running(self):
        self.step(0)
        self.step(50)
        self.assertEqual(self.step(100, halted=1), (1, 0, False, False, True))
        self.step(200, halted=0)  # Kernel's first100ms high then100ms low.
        self.step(290, halted=0)
        self.step(300, halted=1)
        self.assertEqual(self.step(320, halted=1), (3, 0, False, True, True))

    def test_cut_ignored_by_hardware_does_not_repeat(self):
        self.step(0, request=1)
        self.step(100)
        self.step(110, halted=1)
        self.step(130, halted=1)
        self.step(150, halted=1)
        self.assertEqual(self.step(2150, halted=1), (6, 5, True, False, True))
        self.assertFalse(self.step(3000, request=1, halted=1)[3])

    def test_delayed_gpio_write_starts_physical_pulse_timer(self):
        self.step(0, request=1)
        self.step(100)
        self.lib.fixture_power_applied(self.device, 150, 0, 0)
        self.assertEqual(self.step(60120)[0], 2)
        self.step(60121, halted=1)
        self.step(60141, halted=1)
        self.lib.fixture_power_applied(self.device, 60155, 1, 0)
        self.assertTrue(self.step(60174, halted=1)[3])
        self.assertFalse(self.step(60175, halted=1)[3])

    def test_tick_wrap_does_not_shorten_stop_or_cut(self):
        start = 0xffffffc0
        self.step(start, request=1)
        self.assertEqual(self.step(start + 99)[0], 1)
        self.assertEqual(self.step(start + 100)[0], 2)
        self.step(start + 110, halted=1)
        self.assertTrue(self.step(start + 130, halted=1)[3])
        self.assertTrue(self.step(start + 149, halted=1)[3])
        self.assertFalse(self.step(start + 150, halted=1)[3])
