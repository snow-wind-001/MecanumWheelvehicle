"""Frame and unit conversions between MAVLink (NED/FRD) and ROS REP-103 (ENU/FLU).

The firmware reports every vector in its body frame FRD (x forward, y right,
z down) and every earth vector in NED.  ROS2 consumers expect FLU body axes
and an ENU earth frame, so each conversion below is a pure function with no
ROS imports and can be unit tested on its own.

Example:
    from mecanum_mavlink.conversions import quaternion_ned_to_enu
    q = quaternion_ned_to_enu(1.0, 0.0, 0.0, 0.0)
    assert q == (0.0, 2 ** -0.5, 2 ** -0.5, 0.0)
"""

import math

HALF_SQRT2 = math.sqrt(0.5)

# Earth: ENU <- NED is a +90 deg yaw followed by a z flip; as a quaternion
# that is (0, sqrt(1/2), sqrt(1/2), 0).
_Q_NED_TO_ENU = (0.0, HALF_SQRT2, HALF_SQRT2, 0.0)
# Body: FLU <- FRD is a 180 deg roll, i.e. (0, 1, 0, 0).
_Q_FRD_TO_FLU = (0.0, 1.0, 0.0, 0.0)


def wrap_pi(angle):
    """Wrap an angle to (-pi, pi]."""
    return math.atan2(math.sin(angle), math.cos(angle))


def quaternion_multiply(a, b):
    """Hamilton product of two (w, x, y, z) quaternions."""
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw * bw - ax * bx - ay * by - az * bz,
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
    )


def quaternion_normalize(q):
    """Return ``q`` scaled to unit length, or identity for a zero quaternion."""
    w, x, y, z = q
    norm = math.sqrt(w * w + x * x + y * y + z * z)
    if norm < 1e-12:
        return (1.0, 0.0, 0.0, 0.0)
    return (w / norm, x / norm, y / norm, z / norm)


def quaternion_ned_to_enu(q):
    """Convert a body-FRD -> NED attitude quaternion to body-FLU -> ENU.

    The attitude chain is ``ENU <- NED <- body-FRD <- body-FLU``, so both the
    earth rotation and the FRD->FLU body flip are applied; omitting the second
    factor would leave the vehicle 90 degrees nose-left and upside-down.
    """
    q = quaternion_multiply(_Q_NED_TO_ENU, quaternion_normalize(q))
    return quaternion_normalize(quaternion_multiply(q, _Q_FRD_TO_FLU))


def euler_ned_to_enu(roll, pitch, yaw):
    """Convert NED (roll, pitch, yaw) to ENU.

    Both frames use the same body roll sign; pitch and yaw change sign because
    ENU's y axis points left and its yaw increases counter-clockwise.
    """
    return roll, -pitch, wrap_pi(math.pi / 2.0 - yaw)


def quaternion_to_euler(q):
    """Convert a (w, x, y, z) quaternion to ZYX (roll, pitch, yaw)."""
    w, x, y, z = quaternion_normalize(q)
    sinr_cosp = 2.0 * (w * x + y * z)
    cosr_cosp = 1.0 - 2.0 * (x * x + y * y)
    roll = math.atan2(sinr_cosp, cosr_cosp)
    sinp = 2.0 * (w * y - z * x)
    pitch = math.copysign(math.pi / 2.0, sinp) if abs(sinp) >= 1.0 else math.asin(sinp)
    siny_cosp = 2.0 * (w * z + x * y)
    cosy_cosp = 1.0 - 2.0 * (y * y + z * z)
    return roll, pitch, math.atan2(siny_cosp, cosy_cosp)


def euler_to_quaternion(roll, pitch, yaw):
    """Convert ZYX (roll, pitch, yaw) to a (w, x, y, z) quaternion."""
    cr, sr = math.cos(roll / 2.0), math.sin(roll / 2.0)
    cp, sp = math.cos(pitch / 2.0), math.sin(pitch / 2.0)
    cy, sy = math.cos(yaw / 2.0), math.sin(yaw / 2.0)
    return (
        cr * cp * cy + sr * sp * sy,
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
    )


def ned_position_to_enu(north, east, down):
    """Convert an NED position offset to ENU."""
    return east, north, -down


def ned_velocity_to_enu(vn, ve, vd):
    """Convert an earth-frame NED velocity to ENU."""
    return ve, vn, -vd


def body_frd_to_flu(vx, vy, vz):
    """Convert a body-frame FRD vector to FLU."""
    return vx, -vy, -vz


def body_frd_rates_to_flu(gx, gy, gz):
    """Convert body-frame FRD angular rates to FLU."""
    return gx, -gy, -gz


def ned_earth_velocity_to_body_flu(vn, ve, vd, yaw_ned):
    """Rotate an NED velocity into a body FLU frame using the NED yaw.

    Used when only LOCAL_POSITION_NED is available; ODOMETRY already reports
    body FRD velocities and should be preferred.
    """
    cos_yaw, sin_yaw = math.cos(yaw_ned), math.sin(yaw_ned)
    forward = vn * cos_yaw + ve * sin_yaw
    right = -vn * sin_yaw + ve * cos_yaw
    return forward, -right, -vd


def enu_twist_to_body_ned(linear_x, linear_y, angular_z):
    """Convert a ROS FLU ``Twist`` command to MAVLink BODY_NED setpoint components.

    ``Twist`` is x forward, y left, z up with yaw counter-clockwise; MAVLink
    BODY_NED uses y right and yaw clockwise, hence the two negations.  Vertical
    motion is not actuated by the mecanum base and is dropped.
    """
    return linear_x, -linear_y, -angular_z


def clamp(value, limit):
    """Symmetric clamp to +/- ``limit``."""
    if limit <= 0.0:
        return 0.0
    return max(-limit, min(limit, value))


def mavlink_battery_to_ros(voltage_battery, current_battery, battery_remaining):
    """Convert MAVLink battery fields to (voltage_V, current_A, percentage).

    MAVLink encodes unknown values as -1 (current as centiamps), which is
    mapped to ``float('nan')`` so consumers can distinguish them from zero.
    """
    voltage = voltage_battery / 1000.0 if voltage_battery > 0 else float("nan")
    current = current_battery / 100.0 if current_battery >= 0 else float("nan")
    percentage = float(battery_remaining) if battery_remaining >= 0 else float("nan")
    return voltage, current, percentage
