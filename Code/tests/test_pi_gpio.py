"""Check Pi READY ownership and cleanup without accessing host GPIO hardware.

Example: python3 -m unittest discover -s Code/tests -p test_pi_gpio.py -v
"""
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ros2_ws/src/mecanum_bridge'))
from mecanum_bridge.pi_gpio import ReadyLine


class PiGpioTests(unittest.TestCase):
    def test_v2_initial_low_repeated_update_and_release_low(self):
        request = Mock()
        module = SimpleNamespace(request_lines=Mock(return_value=request), LineSettings=Mock(),
                                 line=SimpleNamespace(Direction=SimpleNamespace(OUTPUT='out'),
                                                      Value=SimpleNamespace(ACTIVE=1, INACTIVE=0)))
        with patch('mecanum_bridge.pi_gpio.importlib.import_module', return_value=module):
            line = ReadyLine('/dev/gpiochip0')
            module.LineSettings.assert_called_once_with(direction='out', output_value=0)
            self.assertEqual(set(module.request_lines.call_args.kwargs['config']), {17})
            line.set(True)
            line.set(True)
            request.set_value.assert_called_once_with(17, 1)
            line.close()
            request.set_value.assert_called_with(17, 0)
            request.release.assert_called_once()
            line.close()
            with self.assertRaises(RuntimeError):
                line.set(True)

    def test_v1_initial_low_and_release_even_after_gpio_error(self):
        chip, gpio = Mock(), Mock()
        chip.get_line.return_value = gpio
        module = SimpleNamespace(Chip=Mock(return_value=chip), LINE_REQ_DIR_OUT=3)
        with patch('mecanum_bridge.pi_gpio.importlib.import_module', return_value=module):
            line = ReadyLine('/dev/gpiochip0')
            chip.get_line.assert_called_once_with(17)
            gpio.request.assert_called_once_with(consumer='mecanum-pi-ready', type=3, default_vals=[0])
            line.set(True)
            gpio.set_value.side_effect = OSError('GPIO controller lost')
            with self.assertRaises(OSError):
                line.close()
            gpio.release.assert_called_once()
            chip.close.assert_called_once()

    def test_busy_line_fails_instead_of_claiming_ready(self):
        chip, gpio = Mock(), Mock()
        chip.get_line.return_value = gpio
        gpio.request.side_effect = OSError('Device or resource busy')
        module = SimpleNamespace(Chip=Mock(return_value=chip), LINE_REQ_DIR_OUT=3)
        with patch('mecanum_bridge.pi_gpio.importlib.import_module', return_value=module):
            with self.assertRaises(OSError):
                ReadyLine('/dev/gpiochip0')
        gpio.set_value.assert_not_called()
        chip.close.assert_called_once()
        with self.assertRaises(ValueError):
            ReadyLine('/dev/gpiochip0', offset=22)
