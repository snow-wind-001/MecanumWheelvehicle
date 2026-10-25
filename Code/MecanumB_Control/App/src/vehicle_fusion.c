#include "vehicle_fusion.h"
#include <math.h>
#include <string.h>

#define VF_PI 3.14159265358979f

static float sq(float x) { return x * x; }

void vf_init(VfFusion *f, float beta) {
    memset(f, 0, sizeof(*f));
    f->q[0] = 1.f;
    f->beta = beta > 0.f ? beta : .08f;
    f->accel_tau = .02f;
    f->gyro_tau = .005f;
    f->wheel_gain = .02f;
}

void vf_reset(VfFusion *f) {
    float beta = f->beta;
    vf_init(f, beta);
}

static void initial_attitude(VfFusion *f, const float a[3]) {
    float roll = atan2f(a[1], a[2]);
    float pitch = atan2f(-a[0], sqrtf(sq(a[1]) + sq(a[2])));
    float cr = cosf(roll * .5f), sr = sinf(roll * .5f);
    float cp = cosf(pitch * .5f), sp = sinf(pitch * .5f);
    f->q[0] = cr * cp;
    f->q[1] = sr * cp;
    f->q[2] = cr * sp;
    f->q[3] = -sr * sp;
}

void vf_update(VfFusion *f, const float a[3], const float g[3], float dt) {
    if (!(dt > .0005f && dt < .1f)) return;
    for (unsigned i = 0; i < 3; i++) {
        f->accel[i] += dt / (f->accel_tau + dt) * (a[i] - f->accel[i]);
        f->gyro[i] += dt / (f->gyro_tau + dt) * (g[i] - f->gyro[i]);
    }
    if (!f->initialized) {
        float norm = sqrtf(sq(f->accel[0]) + sq(f->accel[1]) + sq(f->accel[2]));
        if (norm < .1f) return;
        initial_attitude(f, f->accel);
        f->initialized = 1;
        return;
    }
    float q0 = f->q[0], q1 = f->q[1], q2 = f->q[2], q3 = f->q[3];
    float gx = f->gyro[0], gy = f->gyro[1], gz = f->gyro[2];
    /* Wheel yaw rate is a slow, low-drift aid; pull the gyro bias gently so it
     * cannot override real angular motion. vf_set_wheel_rate expects the same
     * sign as gyro z (clockwise seen from above for body z down). */
    if (f->wheel_valid && fabsf(f->wheel_wz) > .05f) {
        gz += f->wheel_gain * (f->wheel_wz - gz);
        f->gyro[2] = gz;
    }
    float qDot0 = .5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot1 = .5f * (q0 * gx + q2 * gz - q3 * gy);
    float qDot2 = .5f * (q0 * gy - q1 * gz + q3 * gx);
    float qDot3 = .5f * (q0 * gz + q1 * gy - q2 * gx);

    float ax = f->accel[0], ay = f->accel[1], az = f->accel[2];
    float norm = sqrtf(sq(ax) + sq(ay) + sq(az));
    if (norm > .1f) {
        ax /= norm; ay /= norm; az /= norm;
        /* Gradient of |R(q)^T (0,0,1) - a|, with the reference gravity pointing
         * along body +z at level. Callers pass minus the accelerometer reading,
         * which is the gravity direction in body FRD (z down). */
        float _2q0 = 2.f * q0, _2q1 = 2.f * q1, _2q2 = 2.f * q2, _2q3 = 2.f * q3;
        float q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;
        float s0 = 4.f * q0 * q2q2 + _2q2 * ax + 4.f * q0 * q1q1 - _2q1 * ay;
        float s1 = 4.f * q1 * q3q3 - _2q3 * ax + 4.f * q0 * q0 * q1 - _2q0 * ay -
                   4.f * q1 + 8.f * q1 * q1q1 + 8.f * q1 * q2q2 + 4.f * q1 * az;
        float s2 = 4.f * q0 * q0 * q2 + _2q0 * ax + 4.f * q2 * q3q3 - _2q3 * ay -
                   4.f * q2 + 8.f * q2 * q1q1 + 8.f * q2 * q2q2 + 4.f * q2 * az;
        float s3 = 4.f * q1q1 * q3 - _2q1 * ax + 4.f * q2q2 * q3 - _2q2 * ay;
        float sn = sqrtf(sq(s0) + sq(s1) + sq(s2) + sq(s3));
        if (sn > 1e-6f) {
            qDot0 -= f->beta * s0 / sn;
            qDot1 -= f->beta * s1 / sn;
            qDot2 -= f->beta * s2 / sn;
            qDot3 -= f->beta * s3 / sn;
        }
    }
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;
    float qn = sqrtf(sq(q0) + sq(q1) + sq(q2) + sq(q3));
    if (qn < 1e-6f) { vf_reset(f); return; }
    f->q[0] = q0 / qn; f->q[1] = q1 / qn; f->q[2] = q2 / qn; f->q[3] = q3 / qn;
}

void vf_set_wheel_rate(VfFusion *f, float wz_frd, int valid) {
    f->wheel_wz = wz_frd;
    f->wheel_valid = (uint8_t)(valid && isfinite(wz_frd));
}

void vf_quaternion(const VfFusion *f, float q[4]) { memcpy(q, f->q, sizeof(f->q)); }

void vf_euler(const VfFusion *f, float e[3]) {
    float q0 = f->q[0], q1 = f->q[1], q2 = f->q[2], q3 = f->q[3];
    e[0] = atan2f(2.f * (q0 * q1 + q2 * q3), 1.f - 2.f * (q1 * q1 + q2 * q2));
    float sp = 2.f * (q0 * q2 - q3 * q1);
    e[1] = asinf(sp > 1.f ? 1.f : sp < -1.f ? -1.f : sp);
    e[2] = atan2f(2.f * (q0 * q3 + q1 * q2), 1.f - 2.f * (q2 * q2 + q3 * q3));
}

float vf_yaw_ned(const VfFusion *f) {
    float e[3];
    vf_euler(f, e);
    return e[2]; /* body z is down, so this is clockwise-positive NED heading */
}
