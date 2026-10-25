"""Bounded protocol v1 codec shared with the STM32 wire contract.

Example: python -m unittest discover -s Code/tests
"""
from dataclasses import dataclass
import struct

HEADER = struct.Struct('<BBHIIQH')
VERSION = 1
MAX_PAYLOAD = 256
MAX_WIRE = 283
HELLO, SYNC, HEARTBEAT, MOTION, ARM, LEASE = 1, 2, 0x10, 0x11, 0x12, 0x13
ODOM, IMU, STATUS, GNSS, LIDAR, PARAM, ACK = 0x20, 0x21, 0x22, 0x23, 0x30, 0x40, 0x7f
DIAGNOSTICS = 0x24
SBUS = 0x25
POWER_STATUS, POWER = 0x26, 0x41
RESPONSE = 1


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xffff
    return crc


def cobs_encode(data):
    out = bytearray([0])
    code_at, code = 0, 1
    for byte in data:
        if byte == 0:
            out[code_at] = code
            code_at, code = len(out), 1
            out.append(0)
        else:
            out.append(byte)
            code += 1
            if code == 255:
                out[code_at] = code
                code_at, code = len(out), 1
                out.append(0)
    out[code_at] = code
    return bytes(out)


def cobs_decode(data):
    out = bytearray()
    pos = 0
    while pos < len(data):
        code = data[pos]
        pos += 1
        if not code or pos + code - 1 > len(data):
            raise ValueError('Invalid COBS block')
        out.extend(data[pos:pos + code - 1])
        pos += code - 1
        if code != 255 and pos < len(data):
            out.append(0)
        if len(out) > 280:
            raise ValueError('Decoded frame exceeds 280 bytes')
    return bytes(out)


def seq_newer(value, previous):
    return 0 < ((value - previous) & 0xffffffff) < 0x80000000


@dataclass(frozen=True)
class Frame:
    kind: int
    session: int
    seq: int
    time_us: int
    payload: bytes = b''
    flags: int = 0

    def encode(self):
        if len(self.payload) > MAX_PAYLOAD:
            raise ValueError('Payload exceeds 256 bytes')
        raw = HEADER.pack(VERSION, self.kind, len(self.payload), self.session,
                          self.seq, self.time_us, self.flags) + self.payload
        return cobs_encode(raw + struct.pack('<H', crc16(raw))) + b'\0'

    @classmethod
    def decode(cls, encoded):
        raw = cobs_decode(encoded)
        if len(raw) < 24:
            raise ValueError('Truncated frame')
        version, kind, length, session, seq, time_us, flags = HEADER.unpack_from(raw)
        if version != VERSION or length > MAX_PAYLOAD or len(raw) != 24 + length:
            raise ValueError('Version/length mismatch')
        if crc16(raw[:-2]) != struct.unpack_from('<H', raw, len(raw) - 2)[0]:
            raise ValueError('CRC mismatch')
        return cls(kind, session, seq, time_us, raw[22:-2], flags)


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.dropping = False
        self.rejected = 0

    def feed(self, data):
        frames = []
        for byte in data:
            if byte:
                if not self.dropping:
                    if len(self.buffer) >= MAX_WIRE - 1:
                        self.dropping = True
                        self.buffer.clear()
                    else:
                        self.buffer.append(byte)
            else:
                if self.dropping:
                    self.rejected += 1
                    self.dropping = False
                elif self.buffer:
                    try:
                        frames.append(Frame.decode(self.buffer))
                    except ValueError:
                        self.rejected += 1
                self.buffer.clear()
        return frames
