"""Validate LD06 raw batches and assemble fresh clockwise revolutions for ROS.

Example: python -m unittest discover -s Code/tests -p test_sensors.py
"""
from dataclasses import dataclass
import math
import struct


def crc8(data):
    value = 0
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = ((value << 1) ^ (0x4d if value & 0x80 else 0)) & 255
    return value


def decode_raw(raw):
    if len(raw) != 47 or raw[:2] != b'\x54\x2c' or crc8(raw[:-1]) != raw[-1]:
        raise ValueError('LD06 header/CRC mismatch')
    speed, start = struct.unpack_from('<HH', raw, 2)
    end, device_ms = struct.unpack_from('<HH', raw, 42)
    if start >= 36000 or end >= 36000 or device_ms >= 30000 or not speed:
        raise ValueError('LD06 angle/time/speed invalid')
    span = (end - start) % 36000
    if not 0 < span < 18000:
        raise ValueError('LD06 span is ambiguous')
    points = [struct.unpack_from('<HB', raw, 6 + 3 * i) for i in range(12)]
    return speed, start / 100, span / 100, device_ms, points


@dataclass
class Scan:
    sample_us: int
    scan_time: float
    ranges: list
    intensities: list


class Assembler:
    def __init__(self, bins=450, minimum=.02, maximum=12., offset=0.):
        if not 36 <= bins <= 4096 or not 0 < minimum < maximum or not math.isfinite(offset):
            raise ValueError('Invalid LD06 scan geometry')
        self.bins, self.minimum, self.maximum, self.offset = bins, minimum, maximum, offset
        self.last_angle = self.last_us = self.last_device_ms = None
        self.device_extended_ms = None
        self.started = False
        self.start_us = 0
        self.rejected = self.resets = 0
        self._clear()

    def _clear(self):
        self.ranges = [float('nan')] * self.bins
        self.intensities = [float('nan')] * self.bins

    def _restart(self):
        self.started = False
        self.last_angle = None
        self._clear()
        self.resets += 1

    def feed_batch(self, payload):
        if not payload or not 1 <= payload[0] <= 4 or len(payload) != 1 + payload[0] * 55:
            raise ValueError('LD06 batch count/length mismatch')
        scans = []
        for packet in range(payload[0]):
            index = 1 + packet * 55
            observed_us = struct.unpack_from('<Q', payload, index)[0]
            try:
                speed, start, span, device_ms, points = decode_raw(payload[index + 8:index + 55])
            except ValueError:
                self.rejected += 1
                self._restart()
                continue
            if self.last_us is not None:
                host_gap = observed_us - self.last_us
                device_gap = (device_ms - self.last_device_ms) % 30000
                half_turn_ms = 180000 / speed
                if (host_gap < 0 or host_gap > 250000 or device_gap >= 15000
                        or device_gap >= half_turn_ms or abs(device_gap - host_gap / 1000) > 1000):
                    self._restart()
                    self.device_extended_ms = device_ms
                else:
                    self.device_extended_ms += device_gap
            else:
                self.device_extended_ms = device_ms
            self.last_us, self.last_device_ms = observed_us, device_ms
            step = span / 11
            for i, (distance, intensity) in enumerate(points):
                angle = (start + step * i) % 360
                sample_us = max(0, observed_us - round((11 - i) * step / speed * 1e6))
                if self.last_angle is not None:
                    delta = angle - self.last_angle
                    if delta < -180:
                        if self.started and sample_us > self.start_us:
                            scans.append(Scan(self.start_us, (sample_us - self.start_us) * 1e-6,
                                              self.ranges, self.intensities))
                        self._clear()
                        self.started, self.start_us = True, sample_us
                    elif delta < -1e-6 or delta > 180:
                        self._restart()
                self.last_angle = angle
                if self.started:
                    # LD06 increases clockwise; ROS angles increase CCW.
                    ros_angle = (-math.radians(angle) + self.offset) % (2 * math.pi)
                    bin_index = round(ros_angle * self.bins / (2 * math.pi)) % self.bins
                    meters = distance * .001
                    old = self.ranges[bin_index]
                    if self.minimum <= meters <= self.maximum and (math.isnan(old) or meters < old):
                        self.ranges[bin_index] = meters
                        self.intensities[bin_index] = float(intensity)
        return scans
