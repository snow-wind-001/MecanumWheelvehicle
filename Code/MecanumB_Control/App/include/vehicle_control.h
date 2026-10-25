#ifndef VEHICLE_CONTROL_H
#define VEHICLE_CONTROL_H
#include <stdint.h>

enum { VC_FL, VC_FR, VC_RL, VC_RR, VC_WHEELS };
typedef struct {
    float radius_m, half_length_m, half_width_m;
    uint32_t counts_per_rev; /* Quadrature counts per output wheel revolution. */
    int8_t encoder_sign[4], motor_sign[4];
    float max_wheel_rad_s, max_wheel_accel_rad_s2;
    float kp, ki, feedforward, duty_limit, speed_filter_s, reversal_rad_s;
} VcConfig;
typedef struct {
    VcConfig config;
    uint32_t last_count[4];
    int64_t total[4];
    float measured[4], target[4], integral[4], duty[4];
    int8_t direction[4];
    uint8_t initialized, configured;
} VcControl;

int vc_config_valid(const VcConfig *c);
void vc_init(VcControl *v, const VcConfig *c);
void vc_inverse(const VcConfig *c, float vx, float vy, float wz, float wheel[4]);
void vc_forward(const VcConfig *c, const float wheel[4], float body[3]);
int64_t vc_encoder_delta(uint32_t now, uint32_t previous, unsigned bits);
/* dt seconds; encoder counts sampled synchronously, 32/16/16/32 bits. */
void vc_step(VcControl *v, const uint32_t counts[4], const float body[3], float dt, int permitted);
#endif
