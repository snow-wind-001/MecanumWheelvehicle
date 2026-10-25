"""ROS 2 node for the dual-board vehicle's protocol v1 serial link.

Example: ros2 run mecanum_bridge bridge --ros-args -p port:=/dev/serial0
"""
import math
import struct
import time
from collections import deque

import serial
import rclpy
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.node import Node
from rclpy.task import Future
from geometry_msgs.msg import Twist, TransformStamped
from nav_msgs.msg import Odometry as OdometryMsg
from sensor_msgs.msg import Imu, JointState, NavSatFix, NavSatStatus, BatteryState, LaserScan, Temperature
from std_msgs.msg import UInt16MultiArray
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from std_srvs.srv import SetBool, Trigger
from tf2_ros import TransformBroadcaster

from . import protocol as p
from .client import Client
from .odometry import Odometry
from .lidar import Assembler
from .pi_gpio import ReadyLine


class Bridge(Node):
    def __init__(self):
        super().__init__('mecanum_bridge')
        defaults = dict(port='/dev/serial0', baud=921600, ready_gpiochip='', base_frame='base_link', odom_frame='odom',
                        imu_frame='imu_link', gnss_frame='gnss_link', publish_tf=True,
                        lidar_frame='lidar_link', lidar_bins=450, lidar_min_m=.02, lidar_max_m=12., lidar_offset_rad=0.,
                        radius_m=0., half_length_m=0., half_width_m=0., counts_per_rev=0,
                        encoder_sign=[1, 1, 1, 1], motor_sign=[1, 1, 1, 1], max_wheel_rad_s=10.,
                        max_wheel_accel_rad_s2=20., kp=0.02, ki=0.1, feedforward=0., duty_limit=0.2,
                        speed_filter_s=0.01, reversal_rad_s=0.5)
        for name, value in defaults.items():
            self.declare_parameter(name, value)
        self.ready_line = None
        chip = self.get_parameter('ready_gpiochip').value
        if chip:
            try:
                self.ready_line = ReadyLine(chip)
            except (ImportError, OSError, RuntimeError, AttributeError) as exc:
                self.get_logger().fatal(f'Cannot claim Pi READY GPIO17: {exc}')
                super().destroy_node()
                raise
        self.calibration = {name: self.get_parameter(name).value for name in (
            'radius_m', 'half_length_m', 'half_width_m', 'counts_per_rev', 'encoder_sign', 'motor_sign',
            'max_wheel_rad_s', 'max_wheel_accel_rad_s2', 'kp', 'ki', 'feedforward', 'duty_limit',
            'speed_filter_s', 'reversal_rad_s')}
        self.port = None
        self.next_connect = 0.
        self.tx = bytearray()
        self.services_pending = {}
        self.time_samples = deque(maxlen=20)
        self.odom = None
        self.client = Client(self._queue, self._event)
        self.lidar = Assembler(self.get_parameter('lidar_bins').value, self.get_parameter('lidar_min_m').value,
                               self.get_parameter('lidar_max_m').value, self.get_parameter('lidar_offset_rad').value)
        self.pub_odom = self.create_publisher(OdometryMsg, 'odom', 10)
        self.pub_joint = self.create_publisher(JointState, 'joint_states', 10)
        self.pub_imu = self.create_publisher(Imu, 'imu/data_raw', 10)
        self.pub_temperature = self.create_publisher(Temperature, 'imu/temperature', 10)
        self.pub_gnss = self.create_publisher(NavSatFix, 'fix', 10)
        self.pub_battery = self.create_publisher(BatteryState, 'battery', 10)
        self.pub_scan = self.create_publisher(LaserScan, 'scan', 10)
        self.pub_diagnostic = self.create_publisher(DiagnosticArray, 'diagnostics', 10)
        self.pub_sbus = self.create_publisher(UInt16MultiArray, 'rc/sbus_channels_raw', 10)
        self.tf = TransformBroadcaster(self)
        # Async services wait for actual MCU responses. The group allows serial
        # timer callbacks to keep running while a service coroutine is pending.
        group = ReentrantCallbackGroup()
        self.create_subscription(Twist, 'cmd_vel', self._velocity, 1, callback_group=group)
        self.create_service(SetBool, '~/arm', self._arm, callback_group=group)
        self.create_service(Trigger, '~/configure', self._configure, callback_group=group)
        self.create_service(Trigger, '~/clear_fault', self._clear, callback_group=group)
        self.create_service(Trigger, '~/shutdown_pi', self._shutdown, callback_group=group)
        self.create_timer(.005, self._poll, callback_group=group)
        self.get_logger().info('Serial bridge starts DISARMED; configure measured parameters, then explicitly arm.')

    def _queue(self, wire):
        if len(self.tx) + len(wire) > 4096:
            raise OSError('Serial TX backlog exceeds 4096 bytes')
        self.tx.extend(wire)

    def _velocity(self, msg):
        try:
            self.client.command_velocity(msg.linear.x, msg.linear.y, msg.angular.z, time.monotonic())
        except ValueError as exc:
            # An invalid topic value must not prolong the last valid command.
            self.client.last_command = -1e9
            self.get_logger().error(str(exc))

    async def _service(self, operation, action, response):
        if self.services_pending:
            response.success, response.message = False, 'Another management request is pending'
            return response
        future = Future()
        self.services_pending[operation] = future
        try:
            action()
        except (ValueError, OSError) as exc:
            self.services_pending.pop(operation, None)
            response.success, response.message = False, str(exc)
            return response
        response.success, response.message = await future
        return response

    async def _arm(self, request, response):
        return await self._service('arm' if request.data else 'disarm',
                                   lambda: self.client.arm(request.data, time.monotonic()), response)

    async def _configure(self, request, response):
        del request
        # Parameters are read again so a stopped operator can correct calibration.
        for name in self.calibration:
            self.calibration[name] = self.get_parameter(name).value
        return await self._service('configure', lambda: self.client.configure(self.calibration, time.monotonic()), response)

    async def _clear(self, request, response):
        del request
        return await self._service('clear_fault', lambda: self.client.clear_fault(time.monotonic()), response)

    async def _shutdown(self, request, response):
        del request
        return await self._service('shutdown_pi', lambda: self.client.shutdown_pi(time.monotonic()), response)

    def _event(self, operation, value, detail):
        if operation in self.services_pending:
            future = self.services_pending.pop(operation)
            if operation == 'configure' and value:
                c = self.calibration
                self.odom = Odometry(c['radius_m'], c['half_length_m'], c['half_width_m'], c['counts_per_rev'])
            future.set_result((bool(value), str(detail)))
        if operation == 'connected':
            self.get_logger().info(f'MCU session established, boot ID {detail}; explicit configure/arm required')
        elif operation == 'disconnected':
            if self.ready_line:
                self.ready_line.set(False)
            self.odom = None
            self.time_samples.clear()
            self.lidar = Assembler(self.lidar.bins, self.lidar.minimum, self.lidar.maximum, self.lidar.offset)
            self.tx.clear()
            if self.port is not None:
                self._queue(b'\0')
            self.get_logger().warning(str(detail))
        elif operation == 'sync':
            self.time_samples.append((float(detail), float(value)))
        elif operation == 'telemetry':
            try:
                self._telemetry(value, detail)
            except (ValueError, struct.error, OverflowError) as exc:
                self.get_logger().warning(f'Rejected telemetry: {exc}')

    def _stamp(self, sample_us, host_now):
        now_ns = self.get_clock().now().nanoseconds
        if self.time_samples:
            uncertainty, offset = min(self.time_samples)
            del uncertainty
            age = host_now - (sample_us - offset) * 1e-6
            # Reject nonsensical/old timestamps instead of publishing future data.
            if not -.1 <= age <= 2:
                raise ValueError('MCU sample timestamp outside synchronization window')
            now_ns -= round(age * 1e9)
        from rclpy.time import Time
        return Time(nanoseconds=max(0, now_ns)).to_msg()

    def _telemetry(self, frame, host_now):
        data = frame.payload
        if frame.kind == p.ODOM:
            if len(data) != 64:
                raise ValueError('ODOM must have 64 payload bytes')
            sample, *values = struct.unpack('<Q4q4iIHH', data)
            counts, speeds, valid = values[:4], [v * .001 for v in values[4:8]], values[9]
            if valid & 15 != 15 or self.odom is None or not self.odom.update(counts, sample):
                return
            stamp = self._stamp(sample, host_now)
            msg = OdometryMsg()
            msg.header.stamp, msg.header.frame_id = stamp, self.get_parameter('odom_frame').value
            msg.child_frame_id = self.get_parameter('base_frame').value
            msg.pose.pose.position.x, msg.pose.pose.position.y = self.odom.x, self.odom.y
            msg.pose.pose.orientation.z, msg.pose.pose.orientation.w = math.sin(self.odom.yaw / 2), math.cos(self.odom.yaw / 2)
            vx, vy, wz = self.odom.forward(speeds)
            msg.twist.twist.linear.x, msg.twist.twist.linear.y, msg.twist.twist.angular.z = vx, vy, wz
            # Initial planar uncertainty estimates; tune against physical ground truth.
            for index in (0, 7, 35):
                msg.pose.covariance[index] = .05
                msg.twist.covariance[index] = .05
            for index in (14, 21, 28):
                msg.pose.covariance[index] = msg.twist.covariance[index] = 1e6
            self.pub_odom.publish(msg)
            joint = JointState()
            joint.header = msg.header
            joint.name = ['wheel_fl_joint', 'wheel_fr_joint', 'wheel_rl_joint', 'wheel_rr_joint']
            joint.position = [v * 2 * math.pi / self.odom.cpr for v in counts]
            joint.velocity = speeds
            self.pub_joint.publish(joint)
            if self.get_parameter('publish_tf').value:
                tf = TransformStamped()
                tf.header, tf.child_frame_id = msg.header, msg.child_frame_id
                tf.transform.translation.x, tf.transform.translation.y = self.odom.x, self.odom.y
                tf.transform.rotation = msg.pose.pose.orientation
                self.tf.sendTransform(tf)
        elif frame.kind == p.IMU:
            if len(data) != 48:
                raise ValueError('IMU must have 48 payload bytes')
            sample, ax, ay, az, gx, gy, gz, temperature, sequence, status, reserved = struct.unpack('<Q7i3I', data)
            del sequence
            if not status & 1 or reserved:
                return
            msg = Imu()
            msg.header.stamp, msg.header.frame_id = self._stamp(sample, host_now), self.get_parameter('imu_frame').value
            msg.orientation_covariance[0] = -1.
            msg.angular_velocity.x, msg.angular_velocity.y, msg.angular_velocity.z = gx * 1e-6, gy * 1e-6, gz * 1e-6
            msg.linear_acceleration.x, msg.linear_acceleration.y, msg.linear_acceleration.z = ax * 1e-6, ay * 1e-6, az * 1e-6
            self.pub_imu.publish(msg)
            thermal = Temperature()
            thermal.header, thermal.temperature = msg.header, temperature * .001
            self.pub_temperature.publish(thermal)
        elif frame.kind == p.GNSS:
            if len(data) != 64:
                raise ValueError('GNSS must have 64 payload bytes')
            sample, utc, lat, lon, height, vn, ve, vd, hacc, vacc, sacc, flags, fix, sats, reserved, tow = struct.unpack('<QQ6i4IBBHI', data)
            del utc, vn, ve, vd, sacc, sats, tow
            if reserved:
                raise ValueError('GNSS reserved bits')
            msg = NavSatFix()
            msg.header.stamp, msg.header.frame_id = self._stamp(sample, host_now), self.get_parameter('gnss_frame').value
            msg.status.service = NavSatStatus.SERVICE_GPS
            valid = bool(flags & 1 and fix in (3, 4))
            msg.status.status = NavSatStatus.STATUS_FIX if valid else NavSatStatus.STATUS_NO_FIX
            msg.latitude, msg.longitude, msg.altitude = (lat * 1e-7, lon * 1e-7, height * .001) if valid else (float('nan'),) * 3
            if valid:
                msg.position_covariance[0] = msg.position_covariance[4] = (hacc * .001) ** 2
                msg.position_covariance[8] = (vacc * .001) ** 2
                msg.position_covariance_type = NavSatFix.COVARIANCE_TYPE_APPROXIMATED
            self.pub_gnss.publish(msg)
        elif frame.kind == p.STATUS:
            if len(data) != 32:
                raise ValueError('STATUS must have 32 payload bytes')
            sample, vbat, vm, current, soc, temperature, faults, state, source, sbus_age, cmd_age, io_flags = struct.unpack('<QHHiHhIBBHHH', data)
            del vm, temperature
            stamp = self._stamp(sample, host_now)
            status = DiagnosticStatus()
            status.name, status.hardware_id = 'mecanum/control', 'STM32F429ZG'
            status.level = DiagnosticStatus.ERROR if faults else DiagnosticStatus.OK
            status.message = f'state={state}, source={source}, faults=0x{faults:08x}'
            status.values = [KeyValue(key='cmd_age_ms', value=str(cmd_age)), KeyValue(key='sbus_age_ms', value=str(sbus_age)),
                             KeyValue(key='protocol_rejected', value=str(self.client.parser.rejected))]
            io_names = ('pi_ready', 'pi_halted', 'pi_power_good', 'level_shift_enable_request', 'pi_shutdown_requested')
            status.values.extend(KeyValue(key=name, value=str(bool(io_flags & (1 << bit))) if io_flags & 0x8000 else 'unknown')
                                 for bit, name in enumerate(io_names))
            diagnostic = DiagnosticArray()
            diagnostic.header.stamp, diagnostic.status = stamp, [status]
            self.pub_diagnostic.publish(diagnostic)
            battery = BatteryState()
            battery.header.stamp = stamp
            battery.voltage = vbat * .001 if vbat != 65535 else float('nan')
            battery.current = -current * .001 if current != 0x7fffffff else float('nan')  # ROS charging positive.
            battery.percentage = soc * .001 if soc != 65535 else float('nan')
            battery.charge = battery.capacity = battery.design_capacity = battery.temperature = float('nan')
            battery.present = vbat != 65535
            self.pub_battery.publish(battery)
        elif frame.kind == p.LIDAR:
            for scan in self.lidar.feed_batch(data):
                msg = LaserScan()
                msg.header.stamp, msg.header.frame_id = self._stamp(scan.sample_us, host_now), self.get_parameter('lidar_frame').value
                msg.angle_min = 0.
                msg.angle_increment = 2 * math.pi / self.lidar.bins
                msg.angle_max = msg.angle_increment * (self.lidar.bins - 1)
                msg.range_min, msg.range_max = self.lidar.minimum, self.lidar.maximum
                msg.scan_time, msg.time_increment = scan.scan_time, 0.
                # Bins are sorted CCW, opposite acquisition order. Zero explicitly
                # avoids fabricating positive ray timing for motion compensation.
                msg.ranges, msg.intensities = scan.ranges, scan.intensities
                self.pub_scan.publish(msg)
        elif frame.kind == p.SBUS:
            if len(data) != 48:
                raise ValueError('SBUS must have 48 payload bytes')
            sample, *values = struct.unpack('<Q16HII', data)
            channels, sequence, flags = values[:16], values[16], values[17]
            usable = flags & 3 == 3 and flags & 28 == 0
            if usable:
                # Preserve raw receiver values; radio calibration and mapping are separate.
                raw = UInt16MultiArray()
                raw.data = channels
                self.pub_sbus.publish(raw)
            status = DiagnosticStatus()
            status.name, status.hardware_id = 'mecanum/sbus', 'STM32F429ZG/UART7'
            status.level = DiagnosticStatus.OK if usable else DiagnosticStatus.WARN
            status.message = 'fresh receiver channels' if usable else 'receiver unavailable, stale or reports loss'
            status.values = [KeyValue(key='flags', value=str(flags)),
                             KeyValue(key='sequence', value=str(sequence)),
                             KeyValue(key='sample_us', value=str(sample)),
                             KeyValue(key='digital_ch17', value=str((flags >> 8) & 1)),
                             KeyValue(key='digital_ch18', value=str((flags >> 9) & 1))]
            msg = DiagnosticArray()
            msg.header.stamp, msg.status = self._stamp(frame.time_us, host_now), [status]
            self.pub_diagnostic.publish(msg)
        elif frame.kind == p.DIAGNOSTICS:
            if len(data) != 64:
                raise ValueError('DIAGNOSTICS must have 64 payload bytes')
            sample, *values = struct.unpack('<Q14I', data)
            names = ('sensor_health', 'imu_frames', 'imu_errors', 'gnss_frames', 'gnss_errors',
                     'lidar_frames', 'lidar_errors', 'uart_losses', 'tx_dropped', 'tx_coalesced',
                     'adc_blocks', 'gnss_phase', 'host_uart_errors', 'control_overruns')
            status = DiagnosticStatus()
            status.name, status.hardware_id = 'mecanum/sensors', 'STM32F429ZG'
            status.level = (DiagnosticStatus.ERROR if values[0] & 64 else
                            DiagnosticStatus.OK if values[0] & 15 == 15 else DiagnosticStatus.WARN)
            status.message = ('LD06 branch fault latched; local power disabled' if values[0] & 64 else
                              f'sensor health mask 0x{values[0]:x}')
            status.values = [KeyValue(key=key, value=str(value)) for key, value in zip(names, values)]
            status.values.extend([KeyValue(key='host_lidar_rejected', value=str(self.lidar.rejected)),
                                  KeyValue(key='host_lidar_resets', value=str(self.lidar.resets)),
                                  KeyValue(key='ld06_enable_request', value=str(bool(values[0] & 16))),
                                  KeyValue(key='aux_pgood', value=str(bool(values[0] & 32))),
                                  KeyValue(key='ld06_branch_fault_latched', value=str(bool(values[0] & 64)))])
            msg = DiagnosticArray()
            msg.header.stamp, msg.status = self._stamp(sample, host_now), [status]
            self.pub_diagnostic.publish(msg)

        elif frame.kind == p.POWER_STATUS:
            if len(data) != 16:
                raise ValueError('POWER_STATUS must have16 payload bytes')
            sample, state, fault, flags, elapsed = struct.unpack('<QBBHI', data)
            names = ('idle', 'wait_stop', 'wait_halt', 'cut_pulse', 'wait_off', 'off', 'fault')
            faults = ('none', 'stop_timeout', 'halt_timeout', 'supply_lost', 'halted_stuck', 'cut_timeout')
            if state >= len(names) or fault > 5 or flags & ~0x7f:
                raise ValueError('Invalid Pi power state, fault or flags')
            status = DiagnosticStatus()
            status.name, status.hardware_id = 'mecanum/pi_power', 'STM32F429ZG'
            status.level = DiagnosticStatus.ERROR if fault else DiagnosticStatus.WARN if state else DiagnosticStatus.OK
            status.message = f'{names[state]}; fault={faults[fault]}'
            status.values = [KeyValue(key='state', value=names[state]), KeyValue(key='fault', value=str(fault)),
                             KeyValue(key='fault_name', value=faults[fault]), KeyValue(key='stage_elapsed_ms', value=str(elapsed))]
            keys = ('shutdown_request', 'cut_request', 'motion_inhibit', 'pi_power_good',
                    'pi_halted', 'encoders_still', 'motors_off')
            status.values.extend(KeyValue(key=key, value=str(bool(flags & (1 << bit)))) for bit, key in enumerate(keys))
            msg = DiagnosticArray()
            msg.header.stamp, msg.status = self._stamp(sample, host_now), [status]
            self.pub_diagnostic.publish(msg)

    def _poll(self):
        now = time.monotonic()
        try:
            if self.port is None:
                if now < self.next_connect:
                    return
                self.port = serial.Serial(self.get_parameter('port').value, self.get_parameter('baud').value,
                                          timeout=0, write_timeout=0)
                self.tx.clear()
                self._queue(b'\0')  # Delimit any partial wire frame from the old connection.
            data = self.port.read(min(8192, self.port.in_waiting))
            if data:
                self.client.feed(data, now)
            self.client.tick(now)
            if self.tx:
                sent = self.port.write(self.tx)
                del self.tx[:sent]
        except (serial.SerialException, OSError) as exc:
            if self.port:
                self.port.close()
            self.port = None
            self.tx.clear()
            self.client.disconnect(str(exc))
            self.next_connect = now + 1.
        if self.ready_line:
            self.ready_line.set(self.client.connected and self.client.last_rx is not None and
                                0 <= now - self.client.last_rx < .3)

    def destroy_node(self):
        try:
            if self.ready_line:
                self.ready_line.close()
        finally:
            if self.port:
                self.port.close()
            super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = Bridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()
