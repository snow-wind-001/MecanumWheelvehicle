"""Integrate normalized X-roller wheel counts in ROS x-forward/y-left coordinates.

Example: python -m unittest discover -s Code/tests
"""
import math


class Odometry:
    def __init__(self, radius, half_length, half_width, counts_per_rev):
        self.radius, self.k, self.cpr = radius, half_length + half_width, counts_per_rev
        self.x = self.y = self.yaw = 0.
        self.previous = None
        self.previous_us = None

    def forward(self, wheel):
        a, b, c, d = wheel
        scale = self.radius / 4
        return scale * (a + b + c + d), scale * (-a + b + c - d), scale * (-a + b - c + d) / self.k

    def update(self, counts, sample_us):
        if self.cpr <= 0 or self.radius <= 0 or self.k <= 0:
            raise ValueError('Odometry requires valid calibration')
        if self.previous_us is not None and sample_us <= self.previous_us:
            return False
        if self.previous is not None:
            increments = [(new - old) * 2 * math.pi / self.cpr for new, old in zip(counts, self.previous)]
            dx, dy, turn = self.forward(increments)
            if abs(turn) < 1e-8:
                a, b = 1., turn / 2
            else:
                a, b = math.sin(turn) / turn, (1 - math.cos(turn)) / turn
            bx, by = a * dx - b * dy, b * dx + a * dy
            self.x += math.cos(self.yaw) * bx - math.sin(self.yaw) * by
            self.y += math.sin(self.yaw) * bx + math.cos(self.yaw) * by
            self.yaw = math.atan2(math.sin(self.yaw + turn), math.cos(self.yaw + turn))
        self.previous, self.previous_us = tuple(counts), sample_us
        return True
