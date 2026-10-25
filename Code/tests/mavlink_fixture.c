/* Test-only ABI for the MAVLink adapter and the attitude estimator.
 *
 * Example: gcc -shared -fPIC ... mavlink_fixture.c vehicle_mavlink.c
 */
#include "vehicle_mavlink.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    VlLink link;
    VmContext vm;
    VmInput in;
    VfFusion fusion;
    uint8_t configured;
} MavFixture;

void *mav_create(void) {
    MavFixture *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    vl_init(&f->link, 0x12345678u);
    vl_pi_inputs(&f->link, 1, 1, 0);
    vm_init(&f->vm, &f->link, 1000000u);
    vf_init(&f->fusion, .08f);
    f->configured = 1;
    f->in.configured = 1;
    f->in.battery_valid = 1;
    f->in.vbat_mv = 12000;
    f->in.vmot_mv = 11900;
    return f;
}

void mav_destroy(void *ctx) { free(ctx); }

void mav_feed(void *ctx, unsigned ch, const uint8_t *data, size_t len, uint64_t us) {
    MavFixture *f = ctx;
    vm_rx(&f->vm, ch, data, len, us);
}

void mav_lost(void *ctx, unsigned ch, uint64_t us) {
    MavFixture *f = ctx;
    vm_link_lost(&f->vm, ch, us);
}

void mav_tick(void *ctx, uint64_t us) {
    MavFixture *f = ctx;
    /* One safety tick, as the firmware's 2 ms SafetyTask does. */
    vl_tick(&f->link, us, 0, 1, 1);
    f->in.state = f->link.state;
    f->in.source = f->link.source;
    f->in.configured = f->configured;
    f->in.pi_ready = f->link.pi_ready;
    f->in.power_inhibit = f->link.power_inhibit;
    f->in.fault_bits = f->link.fault_bits;
    vm_tick(&f->vm, &f->in, us);
}

int mav_pop(void *ctx, unsigned ch, uint8_t *dst, int capacity) {
    return (int)vm_tx_pop(&((MavFixture *)ctx)->vm, ch, dst, (uint16_t)capacity);
}

unsigned mav_state(void *ctx) { return ((MavFixture *)ctx)->link.state; }
unsigned mav_source(void *ctx) { return ((MavFixture *)ctx)->link.source; }
int mav_alive(void *ctx, unsigned ch) { return vm_alive(&((MavFixture *)ctx)->vm, ch); }
int mav_power_inhibit(void *ctx) { return ((MavFixture *)ctx)->link.power_inhibit; }
int mav_shutdown_requested(void *ctx) { return ((MavFixture *)ctx)->link.shutdown_requested; }
uint32_t mav_config_seq(void *ctx) { return ((MavFixture *)ctx)->link.config_store_seq; }
uint64_t mav_last_command(void *ctx) { return ((MavFixture *)ctx)->link.last_command_us; }
uint32_t mav_rx_messages(void *ctx, unsigned ch) {
    return ((MavFixture *)ctx)->vm.rx_messages[ch];
}

void mav_body(void *ctx, float out[3]) {
    memcpy(out, ((MavFixture *)ctx)->link.body, sizeof(((MavFixture *)ctx)->link.body));
}

void mav_set_wheel(void *ctx, float vx, float vy, float wz_ccw) {
    MavFixture *f = ctx;
    f->in.body_measured[0] = vx;
    f->in.body_measured[1] = vy;
    f->in.body_measured[2] = wz_ccw;
}
void mav_set_health(void *ctx, const float current[4], float ntc, int ntc_valid,
                    float soc, int soc_valid) {
    MavFixture *f = ctx;
    if (current) {
        memcpy(f->in.current_a, current, sizeof(f->in.current_a));
        f->in.current_valid = 1;
    } else {
        f->in.current_valid = 0;
    }
    f->in.ntc_c = ntc;
    f->in.ntc_valid = (uint8_t)(ntc_valid != 0);
    f->in.soc = soc;
    f->in.soc_valid = (uint8_t)(soc_valid != 0);
    f->in.stack_free[0] = 111;
    f->in.stack_free[1] = 222;
    f->in.jitter_last_us = 7;
    f->in.jitter_max_us = 9;
    f->in.cpu_percent = 42;
    f->in.config_seq = 3;
    f->in.config_errors = 1;
    f->in.stall_mask = 1;
}

int mav_sensor_imu(void *ctx, float ax, float ay, float az, float gx, float gy, float gz,
                   uint64_t sample_us) {
    MavFixture *f = ctx;
    uint8_t payload[48];
    memset(payload, 0, sizeof(payload));
    vp_put_u64(payload, sample_us);
    for (unsigned i = 0; i < 3; i++) {
        float a = i == 0 ? ax : i == 1 ? ay : az;
        float g = i == 0 ? gx : i == 1 ? gy : gz;
        vp_put_u32(payload + 8 + 4 * i, (uint32_t)(int32_t)(a * 1e6f));
        vp_put_u32(payload + 20 + 4 * i, (uint32_t)(int32_t)(g * 1e6f));
    }
    vp_put_u32(payload + 32, 25000);
    vp_put_u32(payload + 36, 1);
    vp_put_u32(payload + 40, 1);
    return vm_sensor(&f->vm, VP_IMU, payload, sizeof(payload), sample_us);
}

/* Independent instance so the estimator can be driven without the adapter. */
void mav_fusion_reset(void *ctx) { vf_init(&((MavFixture *)ctx)->fusion, .08f); }
void mav_fusion_update(void *ctx, const float a[3], const float g[3], float dt) {
    vf_update(&((MavFixture *)ctx)->fusion, a, g, dt);
}
void mav_fusion_euler(void *ctx, float e[3]) {
    vf_euler(&((MavFixture *)ctx)->fusion, e);
}
float mav_fusion_yaw_ned(void *ctx) { return vf_yaw_ned(&((MavFixture *)ctx)->fusion); }

void mav_maintenance(void *ctx, int busy) {
    ((MavFixture *)ctx)->link.maintenance=(uint8_t)(busy!=0);
}
void mav_disarm(void *ctx, uint64_t now) {
    vl_mav_arm(&((MavFixture *)ctx)->link,VL_RC,0,now);
}
void mav_sbus(void *ctx, uint64_t sampled, uint64_t now, int arm, int failsafe) {
    MavFixture *f=ctx;
    uint8_t payload[48]={0};
    vp_put_u64(payload,sampled);
    for (unsigned i=0;i<16;i++) vp_put_u16(payload+8+2*i,1500);
    vp_put_u16(payload+16,arm ? 2000 : 1000);
    vp_put_u32(payload+44,3u | (failsafe ? 8u : 0u));
    vm_sensor(&f->vm,VP_SBUS,payload,sizeof(payload),now);
}
