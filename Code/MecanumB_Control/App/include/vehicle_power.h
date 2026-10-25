#ifndef VEHICLE_POWER_H
#define VEHICLE_POWER_H
#include <stdint.h>

enum { VW_IDLE, VW_WAIT_STOP, VW_WAIT_HALT, VW_CUT_PULSE, VW_WAIT_OFF, VW_OFF, VW_FAULT };
enum { VW_OK, VW_STOP_TIMEOUT, VW_HALT_TIMEOUT, VW_SUPPLY_LOST, VW_HALTED_STUCK, VW_CUT_TIMEOUT };
typedef struct {
    uint32_t stage_since_ms, stable_since_ms, idle_since_ms;
    uint8_t state, fault, inhibit, request_out, cut_out, stable;
    uint8_t seen_halt_low, external_halt, observed_running, idle_tracking;
} VwPower;
/* Call from the nominal10ms PowerTask; elapsed HAL ticks drive all deadlines.
 * request is an accepted one-shot management request.
 * wheels_still means no encoder transitions; motors_off is actual ARM/MOE state.
 * All deadlines use unsigned differences and tolerate the HAL tick wrap.
 * No forced cut on fault; only a qualified kernel HALTED permits a PD3 edge. */
void vw_step(VwPower *p, uint32_t now_ms, int request, int power_good,
             int halted, int wheels_still, int motors_off);
/* Acknowledge actual GPIO writes; ISR/task preemption before the first output
 * write must not shorten the physical cut pulse or the OS shutdown deadline. */
void vw_applied(VwPower *p, uint32_t now_ms, int previous_request, int previous_cut);
#endif
