#ifndef VEHICLE_LINK_H
#define VEHICLE_LINK_H
#include "vehicle_control.h"
#include "vehicle_protocol.h"

enum { VL_PI=0, VL_WIFI=1, VL_RC=2, VL_SOURCES=3 };
enum { VL_DISARMED=1, VL_ARMING=2, VL_RC_ACTIVE=3, VL_WIFI_ACTIVE=4, VL_PI_ACTIVE=5, VL_FAILSAFE=6, VL_FAULT=7 };
enum { VL_OK, VL_BAD_REQUEST, VL_SESSION, VL_SEQUENCE, VL_DENIED, VL_EXPIRED, VL_RANGE, VL_BUSY };
enum { VL_FAULT_HARDWARE=1u, VL_FAULT_CONTROL=2u, VL_FAULT_RX=4u,
       VL_FAULT_OVERCURRENT=8u, VL_FAULT_STALL=16u, VL_FAULT_OVERTEMP=32u,
       VL_FAULT_UNDERVOLT=64u, VL_FAULT_SENSOR=128u };
typedef struct {
    uint32_t session, last_seq, token;
    uint64_t lease_until_us;
    uint8_t established, seq_valid;
} VlPeer;
typedef struct {
    VlPeer peer[VL_SOURCES];
    uint32_t boot_id, epoch, rng, tx_seq, fault_bits, physical_faults;
    uint32_t config_revision; /* incremented on every accepted calibration change */
    uint32_t config_store_seq, config_store_errors;
    uint64_t command_until_us, last_command_us, arming_since_us, ready_since_us;
    uint64_t config_dirty_since_us;
    float body[3];
    VcConfig config;
    uint8_t configured, state, source, arm_pending, pi_ready, power_inhibit, shutdown_requested;
    uint8_t config_dirty;
    uint8_t shutdown_accepted;
    uint8_t maintenance; /* Serialized with ARM; inhibits every motion source. */
    /* MAVLink adapter: heartbeat liveness per source and the mode flag. */
    uint64_t link_until_us[VL_SOURCES];
    uint8_t mavlink;
} VlLink;

void vl_init(VlLink *v, uint32_t boot_id);
/* Caller serializes packet processing and tick/snapshot access. No HAL/RTOS calls. */
void vl_packet(VlLink *v, unsigned source, const VpFrame *request, uint64_t now, VpFrame *response);
void vl_tick(VlLink *v, uint64_t now, uint32_t physical_faults, int interlock_ready, int wheels_neutral);
void vl_stop(VlLink *v, uint8_t state, uint32_t fault);
/* Physical Pi inputs; caller serializes with packet/tick access. Dropping any
 * condition revokes Pi authorization immediately, without affecting WiFi. */
void vl_pi_inputs(VlLink *v, int power_good, int ready, int halted);
/* PowerTask publishes its inhibit. A new hardware ON cycle may clear it,
 * but never restores previous tokens or authorization. */
void vl_power_update(VlLink *v, int inhibit);
int vl_permitted(const VlLink *v, uint64_t now);
/* MAVLink adapter. A companion becomes "alive" through heartbeats; losing it
 * revokes motion authority. Arm enters the same ARMING/ACTIVE path as VP, and
 * each motion setpoint only shortens the command deadline. Setting vl->mavlink
 * enables the heartbeat checks in vl_permitted/vl_tick. */
#define VL_MAV_LINK_TIMEOUT_US UINT64_C(2000000)
void vl_mav_heartbeat(VlLink *v, unsigned source, uint64_t now);
void vl_mav_link_lost(VlLink *v, unsigned source, uint64_t now);
int vl_mav_arm(VlLink *v, unsigned source, int arm, uint64_t now);
int vl_mav_motion(VlLink *v, unsigned source, const float body[3], uint32_t ttl_ms, uint64_t now);
/* Local SBUS receiver authority. Call on every accepted frame before arming or
 * steering; authority expires 250 ms after the last frame. */
#define VL_RC_LINK_TIMEOUT_US UINT64_C(250000)
void vl_rc_frame(VlLink *v, uint64_t now);
/* PARAM v1: 64 bytes; only accepted while DISARMED. */
int vl_config_decode(const uint8_t *p, size_t len, VcConfig *out);
int vl_config_encode(const VcConfig *c, uint8_t out[64]);
/* Mark the RAM copy dirty; the application persists it after the debounce
 * window, only while DISARMED. */
void vl_config_dirty(VlLink *v, uint64_t now);
int vl_state_active(uint8_t state);
#endif
