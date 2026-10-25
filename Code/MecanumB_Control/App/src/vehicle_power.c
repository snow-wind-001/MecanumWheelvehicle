#include "vehicle_power.h"

static void enter(VwPower *p, uint8_t state, uint32_t now) {
    p->state=state; p->stage_since_ms=now; p->stable=0;
}
static int stable(VwPower *p, uint32_t now, int condition, uint32_t duration) {
    if (!condition) { p->stable=0; return 0; }
    if (!p->stable) { p->stable=1; p->stable_since_ms=now; }
    return (uint32_t)(now-p->stable_since_ms)>=duration;
}
static void fault(VwPower *p, uint8_t reason, uint32_t now) {
    p->fault=reason; p->cut_out=0; p->inhibit=1; enter(p,VW_FAULT,now);
}
void vw_applied(VwPower *p, uint32_t now, int previous_request, int previous_cut) {
    if ((p->state==VW_CUT_PULSE && p->cut_out && !previous_cut) ||
        (p->state==VW_WAIT_HALT && p->request_out && !previous_request))
        p->stage_since_ms=now;
}
void vw_step(VwPower *p, uint32_t now, int request, int good,
             int halted, int still, int motors_off) {
    if (p->state==VW_IDLE) {
        if (!good) { p->observed_running=0; p->idle_tracking=0; }
        else if (!halted) {
            if (!p->idle_tracking) { p->idle_tracking=1; p->idle_since_ms=now; }
            if ((uint32_t)(now-p->idle_since_ms)>=50) p->observed_running=1;
        } else p->idle_tracking=0;
        /* Also service a genuine kernel poweroff initiated locally on the Pi.
         * A high HALTED at MCU boot is never sufficient: first observe low
         * with valid Pi power for50ms, then a new high. */
        int external=good && halted && p->observed_running;
        if (!request && !external) return;
        p->inhibit=1; p->external_halt=(uint8_t)external;
        p->seen_halt_low=(uint8_t)(external || !halted);
        if (!good) { fault(p,VW_SUPPLY_LOST,now); return; }
        if (halted && !external) { fault(p,VW_HALTED_STUCK,now); return; }
        enter(p,VW_WAIT_STOP,now);
    }
    if ((p->state==VW_WAIT_STOP || p->state==VW_WAIT_HALT) && !good) {
        p->request_out=0; fault(p,VW_SUPPLY_LOST,now); return;
    }
    switch (p->state) {
    case VW_WAIT_STOP:
        if ((uint32_t)(now-p->stage_since_ms)>=5000) fault(p,VW_STOP_TIMEOUT,now);
        else if (stable(p,now,still && motors_off,100)) {
            p->request_out=(uint8_t)!p->external_halt;
            enter(p,VW_WAIT_HALT,now);
        }
        break;
    case VW_WAIT_HALT:
        if (!halted) p->seen_halt_low=1;
        if ((uint32_t)(now-p->stage_since_ms)>=60000) fault(p,VW_HALT_TIMEOUT,now);
        else if (stable(p,now,halted && p->seen_halt_low && still && motors_off,20)) {
            p->cut_out=1; enter(p,VW_CUT_PULSE,now);
        }
        break;
    case VW_CUT_PULSE:
        /* Keep the pulse high for20ms even if PGOOD falls immediately. A
         * reset only makes a falling edge, which cannot clear the A latch. */
        if ((uint32_t)(now-p->stage_since_ms)>=20) {
            p->cut_out=0; enter(p,VW_WAIT_OFF,now);
        }
        break;
    case VW_WAIT_OFF:
        if ((uint32_t)(now-p->stage_since_ms)>=2000) fault(p,VW_CUT_TIMEOUT,now);
        else if (stable(p,now,!good,20)) {
            p->request_out=0; enter(p,VW_OFF,now);
        }
        break;
    case VW_OFF:
        /* Release the request before a physical ON-button restart. Require
         * HALTED low again; a backpowered halted Pi must not unlock control. */
        if (stable(p,now,good && !halted,50) && (uint32_t)(now-p->stage_since_ms)>=500) {
            p->inhibit=0; p->observed_running=0; p->idle_tracking=0;
            p->seen_halt_low=0; p->external_halt=0; enter(p,VW_IDLE,now);
        }
        break;
    case VW_FAULT:
        if (!good) p->request_out=0;
        break;
    default: break; /* Faults latch until MCU restart; never retry a cut. */
    }
}
