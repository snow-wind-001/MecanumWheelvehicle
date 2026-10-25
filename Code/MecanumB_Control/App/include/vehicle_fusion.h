#ifndef VEHICLE_FUSION_H
#define VEHICLE_FUSION_H
#include <stdint.h>

/* 6-axis (gyro + accelerometer) Madgwick attitude filter with an optional
 * wheel-yaw aid. Pure C: no HAL, no RTOS, safe for native tests.
 *
 * Frames: body FRD (x forward, y right, z down) as used by MAVLink; positive
 * roll/pitch/yaw follow the right-hand rule about those axes. Because z points
 * down, positive yaw is clockwise seen from above, which is exactly the NED
 * heading convention used by MAVLink.
 */
typedef struct {
    float q[4];              /* w, x, y, z */
    float accel[3], gyro[3]; /* low-pass filtered, m/s^2 and rad/s */
    float beta;              /* Madgwick gain, rad/s */
    float accel_tau, gyro_tau, wheel_gain;
    float wheel_wz;          /* wheel-derived yaw rate, same sign as gyro z */
    float temperature_c;
    uint8_t initialized, wheel_valid;
} VfFusion;

void vf_init(VfFusion *f, float beta);
void vf_reset(VfFusion *f);
/* accel is the gravity direction in body FRD, i.e. minus the accelerometer
 * reading (about (0,0,9.81) m/s^2 at level); gyro is rad/s about FRD axes;
 * dt is seconds. Ignores non-physical dt. */
void vf_update(VfFusion *f, const float accel[3], const float gyro[3], float dt);
/* Wheel yaw rate aid. vc_forward()[2] is CCW-positive about z up; pass its
 * negation so it matches gyro z (positive clockwise about z down). */
void vf_set_wheel_rate(VfFusion *f, float wz_frd, int valid);
void vf_quaternion(const VfFusion *f, float q[4]);
/* Roll/pitch/yaw in radians, NED heading (clockwise-positive) for yaw. */
void vf_euler(const VfFusion *f, float euler[3]);
/* yaw in MAVLink NED convention (clockwise-positive), radians. */
float vf_yaw_ned(const VfFusion *f);
#endif
