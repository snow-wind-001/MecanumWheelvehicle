"""Transport-independent host session, authorization and command deadlines.

Example: python -m unittest discover -s Code/tests
"""
import math
import secrets
import struct
from . import protocol as p


def calibration_payload(config):
    names = ('radius_m', 'half_length_m', 'half_width_m', 'counts_per_rev',
             'max_wheel_rad_s', 'max_wheel_accel_rad_s2', 'kp', 'ki', 'feedforward',
             'duty_limit', 'speed_filter_s', 'reversal_rad_s')
    if any(not math.isfinite(float(config[k])) for k in names):
        raise ValueError('Calibration must be finite')
    if not (0.01 <= config['radius_m'] <= .5 and .01 <= config['half_length_m'] <= 2
            and .01 <= config['half_width_m'] <= 2 and 1 <= config['counts_per_rev'] <= 10000000):
        raise ValueError('Wheel geometry and quadrature counts require measured calibration')
    limits = {'max_wheel_rad_s': (0, 200), 'max_wheel_accel_rad_s2': (0, 1000),
              'kp': (0, 10), 'ki': (0, 100), 'feedforward': (0, 10),
              'duty_limit': (0, 1), 'speed_filter_s': (.001, 1), 'reversal_rad_s': (0, 10)}
    for name, (lo, hi) in limits.items():
        value = config[name]
        if value < lo or value > hi or (name in ('max_wheel_rad_s', 'max_wheel_accel_rad_s2', 'duty_limit') and value == 0):
            raise ValueError(f'Invalid calibration: {name}')
    if int(config['counts_per_rev']) != config['counts_per_rev']:
        raise ValueError('counts_per_rev must be an integer')
    if config['counts_per_rev'] * config['max_wheel_rad_s'] * .002 / (2 * math.pi) >= 32768:
        raise ValueError('Encoder resolution/speed exceeds the 16-bit modular sampling range')
    signs = list(config['encoder_sign']) + list(config['motor_sign'])
    if len(config['encoder_sign']) != 4 or len(config['motor_sign']) != 4 or any(s not in (-1, 1) for s in signs):
        raise ValueError('Four encoder and four motor signs must each be +1 or -1')
    header = struct.pack('<HHIIII8b', 1, 1, round(config['radius_m'] * 1e6),
                         round(config['half_length_m'] * 1e6), round(config['half_width_m'] * 1e6),
                         config['counts_per_rev'], *signs)
    values = [round(config[k] * scale) for k, scale in (
        ('max_wheel_rad_s', 1000), ('max_wheel_accel_rad_s2', 1000), ('kp', 1000),
        ('ki', 1000), ('feedforward', 1000), ('duty_limit', 1000),
        ('speed_filter_s', 1e6), ('reversal_rad_s', 1000))]
    return header + struct.pack('<9I', *values, 0)


class Client:
    def __init__(self, send, event=lambda *args: None, source=0):
        if source not in (0, 1):
            raise ValueError('source must be Pi=0 or WiFi=1')
        self.send_wire, self.event, self.source = send, event, source
        self.mode = 5 if source == 0 else 4
        self.session = secrets.randbits(32) or 1
        self.seq, self.rx_seq = 0, None
        self.connected = self.armed = self.arming = False
        self.token = self.epoch = 0
        self.last_hello = self.last_heartbeat = self.last_motion = self.last_lease = -1e9
        self.last_command = -1e9
        self.last_rx = None
        self.last_sync = -1e9
        self.command = (0., 0., 0.)
        self.pending = {}  # Request seq -> (operation, deadline).
        self.lease_pending = None
        self.configured = False
        self.state, self.faults = 1, 0
        self.parser = p.Parser()

    def _send(self, kind, now, payload=b'', operation=None):
        self.seq = (self.seq + 1) & 0xffffffff
        frame = p.Frame(kind, self.session, self.seq, int(now * 1e6), payload)
        self.send_wire(frame.encode())
        if operation:
            self.pending[self.seq] = (operation, now + .5)
        return self.seq

    def command_velocity(self, vx, vy, wz, now):
        if not all(math.isfinite(v) for v in (vx, vy, wz)) or abs(vx) > 3 or abs(vy) > 3 or abs(wz) > 10:
            raise ValueError('Velocity is nonfinite or outside the protocol range')
        self.command, self.last_command = (vx, vy, wz), now

    def configure(self, config, now):
        if not self.connected or self.armed or self.arming or self.pending:
            raise ValueError('Configure requires a connected, disarmed and idle session')
        self._send(p.PARAM, now, calibration_payload(config), 'configure')

    def arm(self, enable, now):
        if not self.connected or self.pending or self.lease_pending:
            raise ValueError('Session is not ready or another transaction is pending')
        if enable:
            if not self.configured or self.state != 1 or self.faults:
                raise ValueError('Configure and clear faults before arming')
            self.arming, self.armed = True, False
            self.lease_pending = ('arm', now + .5)
        else:
            self.armed = self.arming = False
            self.token = 0
        self._send(p.ARM, now, bytes((int(enable), self.mode, 0, 0)), 'arm' if enable else 'disarm')

    def clear_fault(self, now):
        if not self.connected or self.pending or self.armed or self.arming:
            raise ValueError('Clear faults requires a connected, disarmed and idle session')
        self._send(p.ARM, now, bytes((2, self.mode, 0, 0)), 'clear_fault')

    def shutdown_pi(self, now):
        if self.source != 0 or not self.connected or self.pending or self.lease_pending or self.armed or self.arming or self.state not in (1, 6, 7):
            raise ValueError('Pi shutdown requires a connected, stopped/disarmed and idle session')
        self._send(p.POWER, now, bytes((1, 1, 0, 0)), 'shutdown_pi')

    def _finish_arm(self, success, reason):
        self.arming = False
        self.armed = success
        for seq, (operation, _) in list(self.pending.items()):
            if operation == 'arm':
                del self.pending[seq]
        self.event('arm', success, reason)

    def feed(self, data, now):
        for frame in self.parser.feed(data):
            if frame.session != self.session or (self.rx_seq is not None and not p.seq_newer(frame.seq, self.rx_seq)):
                continue
            self.rx_seq = frame.seq
            self.last_rx = now
            if frame.kind == p.HELLO and frame.flags == p.RESPONSE and len(frame.payload) == 12:
                boot, self.epoch, _ = struct.unpack('<III', frame.payload)
                self.connected = True
                self.event('connected', True, str(boot))
            elif frame.kind == p.LEASE and frame.flags == p.RESPONSE and len(frame.payload) == 16 and self.lease_pending:
                self.token, self.epoch, deadline = struct.unpack('<IIQ', frame.payload)
                if not self.token or deadline <= frame.time_us:
                    self.token = 0
                    continue
                self.last_lease = now
                self.lease_pending = None
                for seq, (op, _) in list(self.pending.items()):
                    if op in ('arm', 'renew'):
                        if op == 'renew':
                            del self.pending[seq]
                        else:
                            self.pending[seq] = ('arm', now + .75)
            elif frame.kind == p.SYNC and frame.flags == p.RESPONSE and len(frame.payload) == 16:
                sent_us, mcu_us = struct.unpack('<QQ', frame.payload)
                received_us = int(now * 1e6)
                if 0 <= received_us - sent_us < 2000000:
                    self.event('sync', mcu_us - (sent_us + received_us) / 2, (received_us - sent_us) / 2)
            elif frame.kind == p.ACK and frame.flags == p.RESPONSE and len(frame.payload) == 8:
                request, result, _ = struct.unpack('<IHH', frame.payload)
                if result == 2:  # MCU reboot/unknown session; require fresh handshake and explicit rearm.
                    self.disconnect('MCU session lost')
                    return
                pending = self.pending.pop(request, None)
                if pending:
                    operation, _ = pending
                    if operation == 'configure':
                        self.configured = result == 0
                    if operation in ('arm', 'renew'):
                        self.lease_pending = None
                        if result != 0:
                            self.armed = self.arming = False
                            self.token = 0
                    if operation == 'arm':
                        self._finish_arm(False, f'MCU rejected arm: {result}')
                    else:
                        detail = ('Shutdown accepted; waiting for kernel HALTED and hardware power cut'
                                  if operation == 'shutdown_pi' and result == 0 else f'MCU result {result}')
                        self.event(operation, result == 0, detail)
            elif frame.kind == p.STATUS and len(frame.payload) == 32:
                self.faults = struct.unpack_from('<I', frame.payload, 20)[0]
                self.state = frame.payload[24]
                if self.arming and self.state == self.mode and self.token and not self.faults:
                    self._finish_arm(True, 'MCU reports active control')
                elif self.state in (6, 7) or self.faults:
                    if self.arming:
                        self._finish_arm(False, 'MCU entered failsafe/fault')
                    self.armed = False
                    self.token = 0
                self.event('telemetry', frame, now)
            elif frame.kind in (p.ODOM, p.IMU, p.GNSS, p.LIDAR, p.DIAGNOSTICS, p.SBUS, p.POWER_STATUS):
                self.event('telemetry', frame, now)

    def disconnect(self, reason):
        if self.arming:
            self._finish_arm(False, reason)
        for _, (operation, _) in list(self.pending.items()):
            self.event(operation, False, reason)
        self.pending.clear()
        self.connected = self.configured = self.armed = self.arming = False
        self.token = 0
        self.lease_pending = None
        self.session = secrets.randbits(32) or 1
        self.seq, self.rx_seq = 0, None
        self.parser = p.Parser()
        self.last_command = self.last_hello = -1e9
        self.last_rx = None
        self.event('disconnected', False, reason)

    def tick(self, now):
        if self.connected and self.last_rx is not None and now - self.last_rx > 1:
            self.disconnect('No valid MCU response for one second')
        for seq, (operation, deadline) in list(self.pending.items()):
            if now >= deadline:
                del self.pending[seq]
                if operation == 'arm':
                    self._finish_arm(False, 'Arm confirmation timed out')
                else:
                    self.event(operation, False, 'Request timed out')
                if operation in ('arm', 'renew'):
                    self.lease_pending = None
                    self.armed = self.arming = False
                    self.token = 0
                    self._send(p.ARM, now, bytes((0, self.mode, 0, 0)))
        if not self.connected:
            if now - self.last_hello >= .5:
                self._send(p.HELLO, now)
                self.last_hello = now
            return
        if now - self.last_heartbeat >= .1:
            self._send(p.HEARTBEAT, now)
            self.last_heartbeat = now
        if now - self.last_sync >= .5:
            self._send(p.SYNC, now, struct.pack('<Q', int(now * 1e6)))
            self.last_sync = now
        if self.armed and now - self.last_command > .15:
            self.armed = False
            self.token = 0
            self._send(p.ARM, now, bytes((0, self.mode, 0, 0)))
            self.event('stopped', False, 'cmd_vel expired; explicit rearm required')
        if (self.armed or self.arming) and self.token and now - self.last_motion >= .02:
            target = self.command if self.armed else (0., 0., 0.)
            payload = struct.pack('<IIHBBiii', self.token, self.epoch, 100, self.mode, 0,
                                  *[round(v * 1000) for v in target])
            self._send(p.MOTION, now, payload)
            self.last_motion = now
        if self.armed and now - self.last_lease >= .4 and not self.lease_pending:
            self._send(p.LEASE, now, struct.pack('<II', self.token, self.epoch), 'renew')
            self.lease_pending = ('renew', now + .5)
