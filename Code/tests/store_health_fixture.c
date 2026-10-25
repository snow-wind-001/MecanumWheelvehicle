/* Test-only ABI for the A/B parameter log and the electrical health module.
 *
 * Example: gcc -shared -fPIC ... store_health_fixture.c vehicle_store.c vehicle_health.c
 */
#include "vehicle_store.h"
#include "vehicle_health.h"
#include <stdlib.h>
#include <string.h>

#define SLOT0 UINT32_C(0x080C0000)
#define SLOT1 UINT32_C(0x080E0000)
#define SLOT_SIZE 4096u

typedef struct {
    uint8_t mem[2][SLOT_SIZE];
    uint32_t erases[2];
    VsStore store;
    VhState health;
    VcConfig config;
} ShFixture;

static unsigned slot_of(uint32_t address) {
    if (address >= SLOT1 && address < SLOT1 + SLOT_SIZE) return 1;
    return 0;
}
static uint32_t base_of(unsigned slot) { return slot ? SLOT1 : SLOT0; }

static int ram_erase(uint32_t address, uint32_t size, void *ctx) {
    ShFixture *f = ctx;
    unsigned slot = slot_of(address);
    (void)size;
    if (address != base_of(slot)) return 0;
    memset(f->mem[slot], 0xff, SLOT_SIZE);
    f->erases[slot]++;
    return 1;
}
static int ram_program(uint32_t address, const void *data, uint32_t len, void *ctx) {
    ShFixture *f = ctx;
    unsigned slot = slot_of(address);
    uint32_t offset = address - base_of(slot);
    if (address < base_of(slot) || (len & 3u) || offset + len > SLOT_SIZE) return 0;
    const uint8_t *bytes=data;
    for (uint32_t i=0;i<len;i++)
        if ((f->mem[slot][offset+i] & bytes[i])!=bytes[i]) return 0;
    for (uint32_t i=0;i<len;i++) f->mem[slot][offset+i]&=bytes[i];
    return 1;
}
static int ram_read(uint32_t address, void *dst, uint32_t len, void *ctx) {
    ShFixture *f = ctx;
    unsigned slot = slot_of(address);
    uint32_t offset = address - base_of(slot);
    if (address < base_of(slot) || offset + len > SLOT_SIZE) return 0;
    memcpy(dst, f->mem[slot] + offset, len);
    return 1;
}

static void default_config(VcConfig *c) {
    memset(c, 0, sizeof(*c));
    c->radius_m = .05f;
    c->half_length_m = .15f;
    c->half_width_m = .12f;
    c->counts_per_rev = 2048;
    for (unsigned i = 0; i < 4; i++) {
        c->encoder_sign[i] = 1;
        c->motor_sign[i] = 1;
    }
    c->max_wheel_rad_s = 20.f;
    c->max_wheel_accel_rad_s2 = 100.f;
    c->kp = .1f;
    c->ki = .2f;
    c->feedforward = .02f;
    c->duty_limit = .5f;
    c->speed_filter_s = .01f;
    c->reversal_rad_s = .5f;
}

void *sh_create(void) {
    ShFixture *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    memset(f->mem, 0xff, sizeof(f->mem));
    default_config(&f->config);
    vh_init(&f->health);
    return f;
}
void sh_destroy(void *ctx) { free(ctx); }

int sh_store_init(void *ctx, int allow_erase) {
    ShFixture *f = ctx;
    const VsSlot slots[2] = {
        {SLOT0, SLOT_SIZE, ram_erase, ram_program, ram_read, f},
        {SLOT1, SLOT_SIZE, ram_erase, ram_program, ram_read, f},
    };
    return vs_store_init(&f->store, slots, allow_erase, &f->config);
}
int sh_store_save(void *ctx) {
    ShFixture *f = ctx;
    return vs_store_save(&f->store, &f->config, 1u);
}
uint32_t sh_store_seq(void *ctx) { return vs_store_seq(&((ShFixture *)ctx)->store); }
uint32_t sh_store_errors(void *ctx) { return vs_store_errors(&((ShFixture *)ctx)->store); }
uint32_t sh_store_writable(void *ctx) { return ((ShFixture *)ctx)->store.writable; }
uint32_t sh_store_active(void *ctx) { return ((ShFixture *)ctx)->store.active; }
uint32_t sh_erase_count(void *ctx, unsigned slot) { return ((ShFixture *)ctx)->erases[slot & 1u]; }
float sh_config_radius(void *ctx) { return ((ShFixture *)ctx)->config.radius_m; }
void sh_config_set_radius(void *ctx, float value) { ((ShFixture *)ctx)->config.radius_m = value; }
void sh_flash_corrupt(void *ctx, unsigned slot, uint32_t offset) {
    ShFixture *f = ctx;
    if (slot < 2 && offset < SLOT_SIZE) f->mem[slot][offset] ^= 0xffu;
}
uint32_t sh_flash_word(void *ctx, unsigned slot, uint32_t offset) {
    ShFixture *f = ctx;
    uint32_t value = 0xffffffffu;
    if (slot < 2 && offset + 4u <= SLOT_SIZE) memcpy(&value, f->mem[slot] + offset, 4);
    return value;
}

void sh_health_init(void *ctx) { vh_init(&((ShFixture *)ctx)->health); }

uint32_t sh_health_step(void *ctx, const uint16_t adc[7], int adc_valid, float wheel,
                        float duty, int permitted, float dt) {
    ShFixture *f = ctx;
    VhInput in;
    memset(&in, 0, sizeof(in));
    if (adc) memcpy(in.adc, adc, sizeof(in.adc));
    in.adc_valid = (uint8_t)(adc_valid != 0);
    for (unsigned i = 0; i < 4; i++) {
        in.wheel_rad_s[i] = wheel;
        in.duty[i] = duty;
    }
    in.permitted = (uint8_t)(permitted != 0);
    vh_step(&f->health, &in, dt);
    return f->health.fault_bits;
}

float sh_current(void *ctx, unsigned i) { return i < 4 ? ((ShFixture *)ctx)->health.current_a[i] : 0.f; }
float sh_vbat(void *ctx) { return (float)((ShFixture *)ctx)->health.vbat_mv; }
float sh_soc(void *ctx) { return ((ShFixture *)ctx)->health.soc; }
int sh_soc_valid(void *ctx) { return ((ShFixture *)ctx)->health.soc_valid; }
int sh_ntc_valid(void *ctx) { return ((ShFixture *)ctx)->health.ntc_valid; }
int sh_stall(void *ctx, unsigned i) { return i < 4 ? ((ShFixture *)ctx)->health.stall[i] : 0; }
int sh_sensor_lost(void *ctx) { return ((ShFixture *)ctx)->health.sensor_lost; }
uint32_t sh_health_faults(void *ctx) { return ((ShFixture *)ctx)->health.fault_bits; }
