#include "vehicle_health.h"
#include <math.h>
#include <string.h>

#define ADC_MAX 4095.0f

float vh_adc_volts(uint16_t adc) { return (float)adc * (VH_VREF_V / ADC_MAX); }

float vh_current_a(uint16_t adc) {
    if (!VH_CURRENT_ENABLED) return 0.f;
    return (vh_adc_volts(adc) - VH_CURRENT_OFFSET_V) / VH_CURRENT_V_PER_A;
}

uint16_t vh_pack_mv(uint16_t adc, float divider) {
    float mv = vh_adc_volts(adc) * divider * 1000.f;
    if (!isfinite(mv) || mv < 0.f) return 0;
    if (mv > 65535.f) return 65535;
    return (uint16_t)(mv + .5f);
}

float vh_ntc_c(uint16_t adc, int *valid) {
    if (valid) *valid = 0;
    if (!VH_NTC_ENABLED) return 0.f;
    float v = vh_adc_volts(adc);
    /* An open or shorted divider is not a temperature reading. */
    if (v <= .01f || v >= VH_VREF_V-.01f) return 0.f;
    float resistance = VH_NTC_PULL_OHM * v / (VH_VREF_V - v);
    if (!(resistance > 0.f) || !isfinite(resistance)) return 0.f;
    float inv = 1.f/298.15f + logf(resistance/VH_NTC_R25_OHM)/VH_NTC_BETA;
    if (!(inv > 0.f)) return 0.f;
    float celsius = 1.f/inv - 273.15f;
    if (celsius < -40.f || celsius > 150.f) return 0.f;
    if (valid) *valid = 1;
    return celsius;
}

float vh_soc(uint16_t vbat_mv) {
    float cell = (float)vbat_mv / (float)VH_BATTERY_CELLS;
    float soc = (cell - (float)VH_CELL_EMPTY_MV) /
                (float)(VH_CELL_FULL_MV - VH_CELL_EMPTY_MV);
    if (!isfinite(soc) || soc < 0.f) return 0.f;
    if (soc > 1.f) return 1.f;
    return soc;
}

void vh_init(VhState *h) { if (h) memset(h,0,sizeof(*h)); }

static uint32_t milliseconds(float dt_s) {
    if (!(dt_s > 0.f) || !isfinite(dt_s)) return 1;
    float ms = dt_s * 1000.f;
    if (ms < 1.f) return 1;
    if (ms > 100.f) return 100;
    return (uint32_t)(ms + .5f);
}

static void timer_add(uint32_t *timer, uint32_t step, uint32_t limit) {
    if (*timer < limit) {
        *timer += step;
        if (*timer > limit) *timer = limit;
    }
}

void vh_step(VhState *h, const VhInput *in, float dt_s) {
    if (!h) return;
    uint32_t step = milliseconds(dt_s);
    if (!in) return;

    h->current_valid = (uint8_t)(in->adc_valid && VH_CURRENT_ENABLED);
    h->voltage_valid = (uint8_t)(in->adc_valid != 0);
    if (h->current_valid)
        for (unsigned i=0;i<4;i++) h->current_a[i] = vh_current_a(in->adc[i]);
    if (h->voltage_valid) {
        h->vbat_mv = vh_pack_mv(in->adc[4], VH_VBAT_DIVIDER);
        h->vmot_mv = vh_pack_mv(in->adc[5], VH_VM_DIVIDER);
        h->soc = vh_soc(h->vbat_mv);
        h->soc_valid = 1;
    } else {
        h->soc_valid = 0;
    }
    int ntc_valid = 0;
    h->ntc_c = vh_ntc_c(in->adc[6], &ntc_valid);
    h->ntc_valid = (uint8_t)(ntc_valid && in->adc_valid && VH_NTC_ENABLED);

    if (h->current_valid) {
        int any_over = 0;
        float low_threshold = VH_OVERCURRENT_A * .5f;
        int all_low = 1;
        for (unsigned i=0;i<4;i++) {
            float magnitude = fabsf(h->current_a[i]);
            if (magnitude > VH_OVERCURRENT_A) any_over = 1;
            if (magnitude > low_threshold) all_low = 0;
        }
        if (any_over) {
            h->overcurrent_clear_ms = 0;
            timer_add(&h->overcurrent_ms, step, VH_OVERCURRENT_MS);
            if (h->overcurrent_ms >= VH_OVERCURRENT_MS) h->overcurrent_latch = 1;
        } else if (h->overcurrent_latch) {
            h->overcurrent_ms = 0;
            if (all_low) timer_add(&h->overcurrent_clear_ms, step, VH_OVERCURRENT_CLEAR_MS);
            else h->overcurrent_clear_ms = 0;
            if (all_low && h->overcurrent_clear_ms >= VH_OVERCURRENT_CLEAR_MS)
                h->overcurrent_latch = 0;
        } else {
            h->overcurrent_ms = 0;
        }

        for (unsigned i=0;i<4;i++) {
            int demanding = in->permitted && fabsf(in->duty[i]) > .1f;
            int stalled = demanding && fabsf(h->current_a[i]) > VH_STALL_A &&
                          fabsf(in->wheel_rad_s[i]) < VH_STALL_SPEED_RAD_S;
            if (stalled) {
                h->stall_clear_ms[i] = 0;
                timer_add(&h->stall_ms[i], step, VH_STALL_MS);
                if (h->stall_ms[i] >= VH_STALL_MS && !h->stall[i]) {
                    h->stall[i] = 1;
                    h->stall_events++;
                }
            } else if (h->stall[i]) {
                h->stall_ms[i] = 0;
                timer_add(&h->stall_clear_ms[i], step, VH_STALL_CLEAR_MS);
                if (h->stall_clear_ms[i] >= VH_STALL_CLEAR_MS) {
                    h->stall[i] = 0;
                    h->stall_clear_ms[i] = 0;
                }
            } else {
                h->stall_ms[i] = 0;
            }
        }
    } else {
        h->overcurrent_ms = h->overcurrent_clear_ms = 0;
        for (unsigned i=0;i<4;i++) h->stall_ms[i] = h->stall_clear_ms[i] = 0;
    }

    if (h->ntc_valid) {
        if (h->ntc_c > VH_TEMP_MAX_C) {
            h->temp_clear_ms = 0;
            timer_add(&h->temp_ms, step, VH_TEMP_MS);
            if (h->temp_ms >= VH_TEMP_MS) h->overtemp_latch = 1;
        } else if (h->overtemp_latch) {
            h->temp_ms = 0;
            if (h->ntc_c < VH_TEMP_RESET_C) timer_add(&h->temp_clear_ms, step, VH_TEMP_CLEAR_MS);
            else h->temp_clear_ms = 0;
            if (h->ntc_c < VH_TEMP_RESET_C && h->temp_clear_ms >= VH_TEMP_CLEAR_MS)
                h->overtemp_latch = 0;
        } else {
            h->temp_ms = 0;
        }
    } else {
        h->temp_ms = h->temp_clear_ms = 0;
    }

    if (h->voltage_valid) {
        if (h->vbat_mv < VH_VBAT_CRITICAL_MV) {
            h->uv_clear_ms = 0;
            timer_add(&h->uv_ms, step, VH_UV_MS);
            if (h->uv_ms >= VH_UV_MS) h->uv_latch = 1;
        } else if (h->uv_latch) {
            h->uv_ms = 0;
            if (h->vbat_mv > VH_VBAT_RECOVER_MV) timer_add(&h->uv_clear_ms, step, VH_UV_CLEAR_MS);
            else h->uv_clear_ms = 0;
            if (h->vbat_mv > VH_VBAT_RECOVER_MV && h->uv_clear_ms >= VH_UV_CLEAR_MS)
                h->uv_latch = 0;
        } else {
            h->uv_ms = 0;
        }
    } else {
        h->uv_ms = h->uv_clear_ms = 0;
    }

    /* Current and voltage protection is blind without the ADC. Refuse to arm
     * after a sustained loss rather than silently dropping the protections. */
    if (VH_REQUIRE_ADC) {
        if (in->adc_valid) h->adc_bad_ms = 0;
        else timer_add(&h->adc_bad_ms, step, VH_ADC_GRACE_MS);
        h->sensor_lost = (uint8_t)(h->adc_bad_ms >= VH_ADC_GRACE_MS);
    } else {
        h->adc_bad_ms = 0;
        h->sensor_lost = 0;
    }

    h->overcurrent = h->overcurrent_latch;
    h->overtemp = h->overtemp_latch;
    h->undervoltage = h->uv_latch;
    uint32_t faults = 0;
    if (h->overcurrent_latch) faults |= VL_FAULT_OVERCURRENT;
    for (unsigned i=0;i<4;i++) if (h->stall[i]) faults |= VL_FAULT_STALL;
    if (h->overtemp_latch) faults |= VL_FAULT_OVERTEMP;
    if (h->uv_latch) faults |= VL_FAULT_UNDERVOLT;
    if (h->sensor_lost) faults |= VL_FAULT_SENSOR;
    h->fault_bits = faults;
}
