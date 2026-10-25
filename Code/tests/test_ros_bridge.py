"""Real ROS service/topic smoke test over PTY using the production C MCU core.

Requires ROS 2 Humble Python; no hardware or motor commands leave the PTY.
Example: ~/.venvs/mecanum-ros2/bin/python -m unittest discover -s Code/tests -p test_ros_bridge.py -v
"""
import ctypes as C
import os
from pathlib import Path
import struct
import sys
import time
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Code/ros2_ws/src/mecanum_bridge'))
try:
    import rclpy
    from rclpy.executors import SingleThreadedExecutor
    from geometry_msgs.msg import Twist
    from nav_msgs.msg import Odometry
    from sensor_msgs.msg import Imu, NavSatFix, LaserScan
    from std_msgs.msg import UInt16MultiArray
    from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
    from std_srvs.srv import SetBool, Trigger
    from mecanum_bridge.node import Bridge
    ROS_AVAILABLE = True
except ImportError:
    ROS_AVAILABLE = False
import test_vehicle as native_tests
from test_sensors import ld06, ubx_nav_pvt, sbus_frame


@unittest.skipUnless(ROS_AVAILABLE, 'Run with the mecanum-ros2 Python environment')
class RosBridgeTests(unittest.TestCase):
    def test_pty_services_motion_odometry_and_sensor_units(self):
        native_tests.FirmwareInteropTests.setUpClass()
        lib = native_tests.FirmwareInteropTests.lib
        lib.fixture_emit.argtypes = [C.c_void_p, C.c_uint8, C.c_char_p, C.c_size_t, C.c_uint64]
        lib.fixture_sensor_feed.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_size_t, C.c_uint64]
        lib.fixture_sbus_report.argtypes = [C.c_void_p, C.c_uint64, C.c_int]
        device = lib.fixture_create()
        master, slave = os.openpty()
        os.set_blocking(master, False)
        port_name = os.ttyname(slave)
        rclpy.init(args=['--ros-args', '-p', f'port:={port_name}', '-p', 'radius_m:=0.05',
                         '-p', 'half_length_m:=0.15', '-p', 'half_width_m:=0.12', '-p', 'counts_per_rev:=2048',
                         '-p', 'ready_gpiochip:=/dev/test-gpiochip'])
        # Only the physical GPIO driver is substituted. Real ROS callbacks,
        # serial/PTY, protocol and MCU state machine still execute.
        gpio = Mock()
        gpio.active = False
        gpio.set.side_effect = lambda active: setattr(gpio, 'active', bool(active))
        gpio.close.side_effect = lambda: gpio.set(False)
        with patch('mecanum_bridge.node.ReadyLine', return_value=gpio) as ready_factory:
            bridge = Bridge()
        ready_factory.assert_called_once_with('/dev/test-gpiochip')
        observer = rclpy.create_node('mecanum_pty_observer')
        executor = SingleThreadedExecutor()
        executor.add_node(bridge)
        executor.add_node(observer)
        received = {'odom': [], 'imu': [], 'fix': [], 'scan': [], 'sbus': [], 'diagnostics': []}
        subscriptions = [observer.create_subscription(cls, topic, lambda msg, key=key: received[key].append(msg), 10)
                         for cls, topic, key in ((Odometry, 'odom', 'odom'), (Imu, 'imu/data_raw', 'imu'), (NavSatFix, 'fix', 'fix'), (LaserScan, 'scan', 'scan'),
                                                 (UInt16MultiArray, 'rc/sbus_channels_raw', 'sbus'), (DiagnosticArray, 'diagnostics', 'diagnostics'))]
        del subscriptions  # Node owns the subscription handles.
        publisher = observer.create_publisher(Twist, 'cmd_vel', 1)
        configure = observer.create_client(Trigger, '/mecanum_bridge/configure')
        arm = observer.create_client(SetBool, '/mecanum_bridge/arm')
        shutdown_pi = observer.create_client(Trigger, '/mecanum_bridge/shutdown_pi')
        out = bytearray()
        start = time.monotonic()
        last_status = last_odom = last_cmd = 0.
        count = 0
        publish_command = True
        mcu_transmit = True

        def drive_once():
            nonlocal last_status, last_odom, last_cmd, count
            now = time.monotonic()
            us = int((now - start) * 1e6)
            executor.spin_once(timeout_sec=.001)
            lib.fixture_pi_inputs(device, 1, int(gpio.active), 0)
            try:
                data = os.read(master, 16384)
            except BlockingIOError:
                data = b''
            if data:
                lib.fixture_feed(device, data, len(data), us)
            lib.fixture_tick(device, us, 0, 1)
            if now - last_status > .05:
                lib.fixture_status(device, us)
                last_status = now
            if now - last_odom > .01:
                if lib.fixture_state(device) == 5:
                    count += 10
                payload = struct.pack('<Q4q4iIHH', us, *([count] * 4), *([1000] * 4), 0, 15, 0)
                lib.fixture_emit(device, 0x20, payload, len(payload), us)
                last_odom = now
            data_buf = C.create_string_buffer(32768)
            n = lib.fixture_read(device, data_buf)
            if mcu_transmit:
                out.extend(data_buf.raw[:n])
            if out:
                try:
                    written = os.write(master, out)
                    del out[:written]
                except BlockingIOError:
                    pass
            if publish_command and now - last_cmd > .02:
                cmd = Twist()
                cmd.linear.x = .1
                publisher.publish(cmd)
                last_cmd = now

        def wait_for(predicate, limit=3.):
            end = time.monotonic() + limit
            while time.monotonic() < end:
                drive_once()
                if predicate():
                    return
            self.fail('ROS/PTY condition timed out')

        try:
            wait_for(lambda: bridge.client.connected and configure.service_is_ready() and arm.service_is_ready())
            self.assertTrue(gpio.active)
            config_future = configure.call_async(Trigger.Request())
            wait_for(config_future.done)
            self.assertTrue(config_future.result().success, config_future.result().message)
            request = SetBool.Request()
            request.data = True
            arm_future = arm.call_async(request)
            wait_for(arm_future.done)
            self.assertTrue(arm_future.result().success, arm_future.result().message)
            self.assertEqual(lib.fixture_state(device), 5)
            wait_for(lambda: any(msg.pose.pose.position.x > 0 for msg in received['odom']))
            us = int((time.monotonic() - start) * 1e6)
            raw = struct.pack('>7h', 0, 0, 0, 8192, 6550, 0, 0)
            lib.fixture_sensor_feed(device, 2, raw, len(raw), us)
            raw = ubx_nav_pvt()
            lib.fixture_sensor_feed(device, 0, raw, len(raw), us)
            wait_for(lambda: received['imu'] and received['fix'])
            self.assertAlmostEqual(received['imu'][-1].linear_acceleration.z, 9.80665)
            self.assertAlmostEqual(received['imu'][-1].angular_velocity.x, 1.745329, places=6)
            self.assertAlmostEqual(received['fix'][-1].latitude, 31.)
            self.assertAlmostEqual(received['fix'][-1].altitude, 12.345)
            wait_for(lambda: time.monotonic() - start > .4)
            us = int((time.monotonic() - start) * 1e6)
            for index in range(80):
                raw = ld06(index)
                lib.fixture_sensor_feed(device, 1, raw, len(raw), us - (80 - index) * 2667)
            wait_for(lambda: received['scan'])
            self.assertEqual(len(received['scan'][-1].ranges), 450)
            self.assertTrue(all(value == 1. for value in received['scan'][-1].ranges))
            us = int((time.monotonic() - start) * 1e6)
            channels = list(range(1000, 1016))
            raw = sbus_frame(channels, flags=3)
            lib.fixture_sensor_feed(device, 3, raw, len(raw), us)
            wait_for(lambda: received['sbus'])
            self.assertEqual(list(received['sbus'][-1].data), channels)
            received['diagnostics'].clear()
            before = len(received['sbus'])
            # Let the shared MCU/host clock actually reach the stale deadline;
            # fabricating a future MCU timestamp races the sync rejection bound.
            wait_for(lambda: int((time.monotonic() - start) * 1e6) >= us + 100000)
            stale_us = int((time.monotonic() - start) * 1e6)
            lib.fixture_sbus_report(device, stale_us, 0)
            wait_for(lambda: any(s.name == 'mecanum/sbus' and s.level == DiagnosticStatus.WARN
                                 for msg in received['diagnostics'] for s in msg.status))
            self.assertEqual(len(received['sbus']), before)
            # Receiver failsafe has fresh bytes but cannot publish usable channels.
            received['diagnostics'].clear()
            raw = sbus_frame(channels, flags=8)
            lib.fixture_sensor_feed(device, 3, raw, len(raw), int((time.monotonic() - start) * 1e6))
            wait_for(lambda: any(s.name == 'mecanum/sbus' and s.level == DiagnosticStatus.WARN
                                 for msg in received['diagnostics'] for s in msg.status))
            self.assertEqual(len(received['sbus']), before)
            received['diagnostics'].clear()
            us = int((time.monotonic() - start) * 1e6)
            diagnostic = struct.pack('<Q14I', us, 0x60, *([0] * 13))
            lib.fixture_emit(device, 0x24, diagnostic, len(diagnostic), us)
            wait_for(lambda: any(s.name == 'mecanum/sensors' and s.level == DiagnosticStatus.ERROR
                                 for msg in received['diagnostics'] for s in msg.status))
            fault_status = next(s for msg in received['diagnostics'] for s in msg.status
                                if s.name == 'mecanum/sensors' and s.level == DiagnosticStatus.ERROR)
            self.assertEqual(dict((v.key, v.value) for v in fault_status.values)['ld06_branch_fault_latched'], 'True')
            received['diagnostics'].clear()
            us = int((time.monotonic() - start) * 1e6)
            status_io = struct.pack('<QHHiHhIBBHHH', us, 65535, 65535, 2147483647,
                                    65535, -32768, 0, 5, 0, 65535, 0, 0x800d)
            lib.fixture_emit(device, 0x22, status_io, len(status_io), us)
            wait_for(lambda: any(s.name == 'mecanum/control' and
                                 dict((v.key, v.value) for v in s.values).get('pi_ready') == 'True'
                                 for msg in received['diagnostics'] for s in msg.status))
            io_status = next(s for msg in received['diagnostics'] for s in msg.status
                             if s.name == 'mecanum/control' and dict((v.key, v.value) for v in s.values).get('pi_ready') == 'True')
            io_values = dict((v.key, v.value) for v in io_status.values)
            self.assertEqual(io_values['pi_halted'], 'False')
            self.assertEqual(io_values['pi_power_good'], 'True')
            self.assertEqual(io_values['level_shift_enable_request'], 'True')
            self.assertEqual(io_values['pi_shutdown_requested'], 'False')
            received['diagnostics'].clear()
            # The fixture's periodic legacy STATUS has no GPIO valid marker.
            wait_for(lambda: any(s.name == 'mecanum/control' and
                                 dict((v.key, v.value) for v in s.values).get('pi_ready') == 'unknown'
                                 for msg in received['diagnostics'] for s in msg.status))
            publish_command = False
            wait_for(lambda: not bridge.client.armed and lib.fixture_state(device) in (1, 6))
            self.assertFalse(lib.fixture_permitted(device, int((time.monotonic() - start) * 1e6)))
            wait_for(shutdown_pi.service_is_ready)
            wait_for(lambda: not bridge.client.pending and not bridge.client.lease_pending and
                     bridge.client.state in (1, 6, 7))
            shutdown_future = shutdown_pi.call_async(Trigger.Request())
            wait_for(shutdown_future.done)
            self.assertTrue(shutdown_future.result().success, shutdown_future.result().message)
            self.assertIn('waiting for kernel HALTED', shutdown_future.result().message)
            ms = int((time.monotonic() - start) * 1000)
            self.assertEqual(lib.fixture_power_step(device, ms, 0, 1, 0, 1, 1) & 255, 1)
            self.assertEqual(lib.fixture_power_step(device, ms + 100, 0, 1, 0, 1, 1) & 255, 2)
            # Exercise missing-HALTED fault from production C and publish it
            # through the production C codec, not a fabricated diagnostic.
            self.assertEqual(lib.fixture_power_step(device, ms + 60100, 0, 1, 0, 1, 1) >> 8 & 255, 2)
            lib.fixture_power_report(device, int((time.monotonic() - start) * 1e6))
            wait_for(lambda: any(s.name == 'mecanum/pi_power' and s.level == DiagnosticStatus.ERROR
                                 for msg in received['diagnostics'] for s in msg.status))
            power_status = next(s for msg in received['diagnostics'] for s in msg.status if s.name == 'mecanum/pi_power')
            power_values = dict((v.key, v.value) for v in power_status.values)
            self.assertEqual(power_values['cut_request'], 'False')
            self.assertEqual(power_values['motion_inhibit'], 'True')
            mcu_transmit = False
            wait_for(lambda: not gpio.active, limit=.8)
            self.assertTrue(bridge.client.connected)  # 300ms READY limit precedes 1s reconnect timeout.
            bridge.client.disconnect('Test transport disconnect')
            self.assertFalse(gpio.active)
        finally:
            executor.remove_node(bridge)
            executor.remove_node(observer)
            bridge.destroy_node()
            gpio.close.assert_called_once()
            self.assertFalse(gpio.active)
            observer.destroy_node()
            executor.shutdown()
            rclpy.try_shutdown()
            os.close(master)
            os.close(slave)
            lib.fixture_destroy(device)
            native_tests.FirmwareInteropTests.tearDownClass()


if __name__ == '__main__':
    unittest.main()
