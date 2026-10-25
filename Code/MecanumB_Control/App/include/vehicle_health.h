#ifndef VEHICLE_HEALTH_H
#define VEHICLE_HEALTH_H
#include <stdint.h>
#include "vehicle_link.h"

/* Electrical health: nominal ADC conversion, per-motor current, NTC
 * temperature, stall/overcurrent/overtemperature/undervoltage protection, and
 * a voltage-based state-of-charge estimate.
 *
 * Every threshold is a compile-time default because the battery pack and the
 * NTC divider are not characterised yet. VH_NTC_ENABLED is off until the external NTC type/Beta and PB1 conversion
 * are verified; enabling it with a guessed divider can
 * report a plausible but wrong temperature and trip a false fault.
 */

#ifndef VH_VREF_V
#define VH_VREF_V 3.3f
#endif
/* STSPIN948: 10 V/V internal amplifier across a 50 mOhm shunt = 0.5 V/A. */
#ifndef VH_CURRENT_V_PER_A
#define VH_CURRENT_V_PER_A 0.5f
#endif
#ifndef VH_CURRENT_OFFSET_V
#define VH_CURRENT_OFFSET_V 0.0f
#endif
#ifndef VH_CURRENT_ENABLED
#define VH_CURRENT_ENABLED 1
#endif
/* 120k/10k divider: (120+10)/10 = 13. */
#ifndef VH_VBAT_DIVIDER
#define VH_VBAT_DIVIDER 13.0f
#endif
#ifndef VH_VM_DIVIDER
#define VH_VM_DIVIDER 13.0f
#endif
#ifndef VH_NTC_ENABLED
#define VH_NTC_ENABLED 0
#endif
#ifndef VH_NTC_PULL_OHM
#define VH_NTC_PULL_OHM 10000.0f
#endif
#ifndef VH_NTC_R25_OHM
#define VH_NTC_R25_OHM 10000.0f
#endif
#ifndef VH_NTC_BETA
#define VH_NTC_BETA 3950.0f
#endif
#ifndef VH_REQUIRE_ADC
#define VH_REQUIRE_ADC 1
#endif
#ifndef VH_BATTERY_CELLS
#define VH_BATTERY_CELLS 4
#endif
#ifndef VH_CELL_FULL_MV
#define VH_CELL_FULL_MV 4200
#endif
#ifndef VH_CELL_EMPTY_MV
#define VH_CELL_EMPTY_MV 3400
#endif

/* Provisional MG370 profile: user-reported 1 A stall current. The board
 * REF=1.5 V, gain=10, Rsense=50 mOhm clamps near 3 A, not 6 A.
 * Validate current offsets, PWM sampling and thresholds on the real motor. */
#ifndef VH_OVERCURRENT_A
#define VH_OVERCURRENT_A       1.2f
#endif
#define VH_OVERCURRENT_MS      20u
#define VH_OVERCURRENT_CLEAR_MS 1000u
#ifndef VH_STALL_A
#define VH_STALL_A             0.8f
#endif
#define VH_STALL_SPEED_RAD_S   1.0f
#define VH_STALL_MS            500u
#define VH_STALL_CLEAR_MS      2000u
#define VH_TEMP_MAX_C          75.0f
#define VH_TEMP_RESET_C        60.0f
#define VH_TEMP_MS             1000u
#define VH_TEMP_CLEAR_MS       2000u
#define VH_VBAT_CRITICAL_MV    (3300u * VH_BATTERY_CELLS)
#define VH_VBAT_RECOVER_MV     (3500u * VH_BATTERY_CELLS)
#define VH_UV_MS               1000u
#define VH_UV_CLEAR_MS         2000u
#define VH_ADC_GRACE_MS        1000u

typedef struct {
    uint16_t adc[7];       /* FL, FR, RL, RR current; VBAT; VM; NTC */
    uint8_t adc_valid;
    float wheel_rad_s[4];
    float duty[4];
    uint8_t permitted;     /* motors are actually allowed to drive */
} VhInput;

typedef struct {
    float current_a[4], ntc_c, soc;
    uint16_t vbat_mv, vmot_mv;
    uint8_t current_valid, voltage_valid, ntc_valid, soc_valid;
    uint8_t overcurrent, overtemp, undervoltage, sensor_lost;
    uint8_t stall[4];
    uint32_t fault_bits, stall_events;
    /* Internal timers. */
    uint32_t overcurrent_ms, overcurrent_clear_ms;
    uint32_t stall_ms[4], stall_clear_ms[4];
    uint32_t temp_ms, temp_clear_ms;
    uint32_t uv_ms, uv_clear_ms, adc_bad_ms;
    uint8_t overcurrent_latch, overtemp_latch, uv_latch;
} VhState;

void vh_init(VhState *h);
void vh_step(VhState *h, const VhInput *in, float dt_s);

/* Pure conversions, exposed for calibration tests. */
float vh_adc_volts(uint16_t adc);
float vh_current_a(uint16_t adc);
uint16_t vh_pack_mv(uint16_t adc, float divider);
float vh_ntc_c(uint16_t adc, int *valid);
float vh_soc(uint16_t vbat_mv);

#endif
