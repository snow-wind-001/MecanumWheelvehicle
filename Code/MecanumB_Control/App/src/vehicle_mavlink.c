#include "vehicle_mavlink.h"
#include "vehicle_protocol.h"
#include "vehicle_sensors.h"
#include "common/mavlink.h"
#include <math.h>
#include <string.h>

/* Fields are packed in MAVLink wire order; keep the enum aligned with
 * vm_context.next_us[]. */
enum {
    VM_R_HEARTBEAT, VM_R_STATUS, VM_R_ATTITUDE, VM_R_IMU,
    VM_R_POSITION, VM_R_ODOMETRY, VM_R_RC, VM_R_BATTERY,
    VM_R_GPS, VM_R_HUD, VM_R_SCAN, VM_R_NAMED, VM_R_COUNT
};
static const uint64_t vm_period[VM_R_COUNT] = {
    VM_HEARTBEAT_PERIOD_US, VM_STATUS_PERIOD_US, VM_ATTITUDE_PERIOD_US,
    VM_IMU_PERIOD_US, VM_POSITION_PERIOD_US, VM_ODOMETRY_PERIOD_US,
    VM_RC_PERIOD_US, VM_BATTERY_PERIOD_US, VM_GPS_PERIOD_US,
    VM_HUD_PERIOD_US, VM_SCAN_PERIOD_US, VM_NAMED_PERIOD_US
};
_Static_assert(VM_R_COUNT == sizeof(((VmContext *)0)->next_us) / sizeof(uint64_t),
               "rate table must match next_us[]");

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int32_t i32_at(const uint8_t *p) { return (int32_t)vp_get_u32(p); }
static float clampf(float x, float lo, float hi) { return fminf(hi, fmaxf(lo, x)); }

static uint64_t ring_used(const VmContext *m, unsigned ch) {
    return (uint64_t)(m->tx_head[ch] - m->tx_tail[ch]);
}

static void ring_push(VmContext *m, unsigned ch, const uint8_t *data, uint16_t len) {
    if (ch >= VM_CHANNELS || !len) return;
    if (ring_used(m, ch) + len > VM_TX_CAPACITY) { m->tx_overflow[ch]++; return; }
    for (uint16_t i = 0; i < len; i++) {
        m->tx_buf[ch][m->tx_head[ch] % VM_TX_CAPACITY] = data[i];
        m->tx_head[ch]++;
    }
}

static void enqueue(VmContext *m, unsigned ch, const mavlink_message_t *msg) {
    uint8_t wire[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(wire, msg);
    ring_push(m, ch, wire, len);
}

static void status_text(VmContext *m, unsigned ch, uint8_t severity, const char *text) {
    if (ch >= VM_CHANNELS) return;
    mavlink_message_t msg;
    mavlink_msg_statustext_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                     severity, text, (uint16_t)(++m->status_seq), 0);
    enqueue(m, ch, &msg);
}

static uint8_t ack_result(int result) {
    switch (result) {
    case VL_OK: return MAV_RESULT_ACCEPTED;
    case VL_EXPIRED: return MAV_RESULT_TEMPORARILY_REJECTED;
    case VL_DENIED: return MAV_RESULT_DENIED;
    case VL_RANGE: return MAV_RESULT_DENIED;
    case VL_BUSY: return MAV_RESULT_TEMPORARILY_REJECTED;
    default: return MAV_RESULT_FAILED;
    }
}

static void command_ack(VmContext *m, unsigned ch, const mavlink_message_t *request,
                        uint16_t command, uint8_t result) {
    mavlink_message_t msg;
    mavlink_msg_command_ack_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                      command, result, 0, 0, request->sysid, request->compid);
    enqueue(m, ch, &msg);
}

static unsigned source_of(unsigned ch) { return ch == VM_CH_ESP ? VL_WIFI : VL_PI; }

/* Calibration parameters exposed through the standard MAVLink parameter
 * protocol. Names fit MAVLink's 16-byte param_id. The vehicle stays unarmable
 * until every field passes vc_config_valid(). */
enum {
    VM_P_RADIUS, VM_P_HALF_LEN, VM_P_HALF_WID, VM_P_COUNTS,
    VM_P_ENC0, VM_P_ENC1, VM_P_ENC2, VM_P_ENC3,
    VM_P_MOT0, VM_P_MOT1, VM_P_MOT2, VM_P_MOT3,
    VM_P_MAX_WHEEL, VM_P_MAX_ACCEL, VM_P_KP, VM_P_KI,
    VM_P_FF, VM_P_DUTY, VM_P_TAU, VM_P_REV, VM_P_COUNT
};
static const char *const vm_param_name[VM_P_COUNT] = {
    "RADIUS_M", "HALF_LEN_M", "HALF_WID_M", "COUNTS_REV",
    "ENC_SIGN0", "ENC_SIGN1", "ENC_SIGN2", "ENC_SIGN3",
    "MOT_SIGN0", "MOT_SIGN1", "MOT_SIGN2", "MOT_SIGN3",
    "MAX_WHEEL", "MAX_ACCEL", "KP", "KI", "FF", "DUTY_LIMIT", "SPEED_TAU", "REVERSAL"
};
static const uint8_t vm_param_type[VM_P_COUNT] = {
    MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32,
    MAV_PARAM_TYPE_INT32,
    MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32,
    MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32, MAV_PARAM_TYPE_INT32,
    MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32,
    MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32, MAV_PARAM_TYPE_REAL32
};

static float vm_param_get(const VcConfig *c, unsigned i) {
    switch (i) {
    case VM_P_RADIUS: return c->radius_m;
    case VM_P_HALF_LEN: return c->half_length_m;
    case VM_P_HALF_WID: return c->half_width_m;
    case VM_P_COUNTS: return (float)c->counts_per_rev;
    case VM_P_ENC0: return c->encoder_sign[0];
    case VM_P_ENC1: return c->encoder_sign[1];
    case VM_P_ENC2: return c->encoder_sign[2];
    case VM_P_ENC3: return c->encoder_sign[3];
    case VM_P_MOT0: return c->motor_sign[0];
    case VM_P_MOT1: return c->motor_sign[1];
    case VM_P_MOT2: return c->motor_sign[2];
    case VM_P_MOT3: return c->motor_sign[3];
    case VM_P_MAX_WHEEL: return c->max_wheel_rad_s;
    case VM_P_MAX_ACCEL: return c->max_wheel_accel_rad_s2;
    case VM_P_KP: return c->kp;
    case VM_P_KI: return c->ki;
    case VM_P_FF: return c->feedforward;
    case VM_P_DUTY: return c->duty_limit;
    case VM_P_TAU: return c->speed_filter_s;
    case VM_P_REV: return c->reversal_rad_s;
    default: return 0.f;
    }
}

static void vm_param_set(VcConfig *c, unsigned i, float v) {
    int8_t sign = (int8_t)(v < 0.f ? -1 : v > 0.f ? 1 : 0);
    switch (i) {
    case VM_P_RADIUS: c->radius_m = v; break;
    case VM_P_HALF_LEN: c->half_length_m = v; break;
    case VM_P_HALF_WID: c->half_width_m = v; break;
    case VM_P_COUNTS: c->counts_per_rev = v > 0.f ? (uint32_t)(v + .5f) : 0u; break;
    case VM_P_ENC0: c->encoder_sign[0] = sign; break;
    case VM_P_ENC1: c->encoder_sign[1] = sign; break;
    case VM_P_ENC2: c->encoder_sign[2] = sign; break;
    case VM_P_ENC3: c->encoder_sign[3] = sign; break;
    case VM_P_MOT0: c->motor_sign[0] = sign; break;
    case VM_P_MOT1: c->motor_sign[1] = sign; break;
    case VM_P_MOT2: c->motor_sign[2] = sign; break;
    case VM_P_MOT3: c->motor_sign[3] = sign; break;
    case VM_P_MAX_WHEEL: c->max_wheel_rad_s = v; break;
    case VM_P_MAX_ACCEL: c->max_wheel_accel_rad_s2 = v; break;
    case VM_P_KP: c->kp = v; break;
    case VM_P_KI: c->ki = v; break;
    case VM_P_FF: c->feedforward = v; break;
    case VM_P_DUTY: c->duty_limit = v; break;
    case VM_P_TAU: c->speed_filter_s = v; break;
    case VM_P_REV: c->reversal_rad_s = v; break;
    default: break;
    }
}

static int vm_param_index(const char *id) {
    for (unsigned i = 0; i < VM_P_COUNT; i++)
        if (strncmp(id, vm_param_name[i], 16) == 0) return (int)i;
    return -1;
}

static void send_param(VmContext *m, unsigned ch, unsigned index) {
    char id[16] = {0};
    strncpy(id, vm_param_name[index], sizeof(id) - 1);
    mavlink_message_t msg;
    mavlink_msg_param_value_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                      id, vm_param_get(&m->link->config, index),
                                      vm_param_type[index], (uint16_t)VM_P_COUNT,
                                      (uint16_t)index);
    enqueue(m, ch, &msg);
}


/* Map raw ICM-42688-P axes to vehicle FRD. */
static void imu_map(const float src[3], float dst[3], int gyro) {
    (void)gyro;
    const unsigned axis[3] = { VM_IMU_AXIS_X, VM_IMU_AXIS_Y, VM_IMU_AXIS_Z };
    const float sign[3] = { VM_IMU_SIGN_X, VM_IMU_SIGN_Y, VM_IMU_SIGN_Z };
    float out[3];
    for (unsigned i = 0; i < 3; i++) out[i] = sign[i] * src[axis[i]];
    memcpy(dst, out, sizeof(out));
}
_Static_assert(VM_IMU_AXIS_X < 3 && VM_IMU_AXIS_Y < 3 && VM_IMU_AXIS_Z < 3,
               "VM_IMU_AXIS_* must index a 3-axis vector");
_Static_assert(VM_IMU_AXIS_X != VM_IMU_AXIS_Y && VM_IMU_AXIS_X != VM_IMU_AXIS_Z &&
               VM_IMU_AXIS_Y != VM_IMU_AXIS_Z, "VM_IMU_AXIS_* must be a permutation");

void vm_init(VmContext *m, VlLink *link, uint64_t now_us) {
    memset(m, 0, sizeof(*m));
    m->link = link;
    m->last_state = link ? link->state : VL_DISARMED;
    vf_init(&m->fusion, .08f);
    for (unsigned i = 0; i < VM_R_COUNT; i++) m->next_us[i] = now_us;
    if (link) link->mavlink = 1;
}

int vm_alive(const VmContext *m, unsigned channel) {
    return channel < VM_CHANNELS && m->alive[channel];
}

uint16_t vm_tx_pop(VmContext *m, unsigned channel, uint8_t *dst, uint16_t capacity) {
    if (channel >= VM_CHANNELS || !dst || !capacity) return 0;
    uint16_t n = 0;
    while (m->tx_tail[channel] != m->tx_head[channel] && n < capacity) {
        dst[n++] = m->tx_buf[channel][m->tx_tail[channel] % VM_TX_CAPACITY];
        m->tx_tail[channel]++;
    }
    return n;
}

void vm_tx_drop(VmContext *m, unsigned channel) {
    if (channel < VM_CHANNELS) m->tx_overflow[channel]++;
}

static void handle_heartbeat(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                             uint64_t now_us) {
    uint8_t type = mavlink_msg_heartbeat_get_type(msg);
    int was_alive = m->alive[ch];
    m->heartbeat_us[ch] = now_us;
    m->remote_type[ch] = type;
    m->alive[ch] = 1;
    vl_mav_heartbeat(m->link, source_of(ch), now_us);
    if (!was_alive)
        status_text(m, ch, MAV_SEVERITY_INFO, ch == VM_CH_PI ? "PI MAVLink up" : "ESP MAVLink up");
}

static void handle_command_long(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                                uint64_t now_us) {
    const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
    uint32_t target_system = mavlink_msg_get_target_sysid(msg, entry);
    uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
    if (target_system && target_system != VM_SYSTEM_ID) return;
    if (target_component && target_component != VM_COMPONENT_ID) return;

    uint16_t command = mavlink_msg_command_long_get_command(msg);
    if (command == MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN) {
        /* The companion may only shut itself down: param1 (autopilot) must be
         * 0 and param2 (companion) must be 2. Accepting this while armed or
         * while a shutdown is already pending would race the power FSM. */
        float autopilot = mavlink_msg_command_long_get_param1(msg);
        float companion = mavlink_msg_command_long_get_param2(msg);
        if (ch==VM_CH_PI && autopilot==0.f && companion==2.f &&
            m->link->shutdown_accepted && m->link->power_inhibit) {
            command_ack(m,ch,msg,command,MAV_RESULT_ACCEPTED);
            return; /* Idempotent retry after a lost ACK; never retrigger the FSM. */
        }
        int allowed = ch == VM_CH_PI && autopilot == 0.f && companion == 2.f &&
                      m->link->pi_ready && !m->link->power_inhibit &&
                      !m->link->arm_pending &&
                      (m->link->state == VL_DISARMED || m->link->state == VL_FAILSAFE ||
                       m->link->state == VL_FAULT);
        if (!allowed) {
            command_ack(m, ch, msg, command, MAV_RESULT_DENIED);
            return;
        }
        vl_power_update(m->link, 1);
        m->link->shutdown_requested = 1;
        m->link->shutdown_accepted = 1;
        command_ack(m, ch, msg, command, MAV_RESULT_ACCEPTED);
        status_text(m, ch, MAV_SEVERITY_WARNING, "PI SHUTDOWN");
        return;
    }
    if (command != MAV_CMD_COMPONENT_ARM_DISARM) {
        command_ack(m, ch, msg, command, MAV_RESULT_UNSUPPORTED);
        return;
    }
    float param1 = mavlink_msg_command_long_get_param1(msg);
    int arm = param1 >= .5f;
    int result = vl_mav_arm(m->link, source_of(ch), arm, now_us);
    command_ack(m, ch, msg, command, ack_result(result));
    if (result != VL_OK) return;
    if (arm) status_text(m, ch, MAV_SEVERITY_INFO, "ARMING");
    else status_text(m, ch, MAV_SEVERITY_INFO, "DISARMED");
}

static void handle_setpoint(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                            uint64_t now_us) {
    const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
    uint32_t target_system = mavlink_msg_get_target_sysid(msg, entry);
    uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
    if (target_system && target_system != VM_SYSTEM_ID) return;
    if (target_component && target_component != VM_COMPONENT_ID) return;

    uint16_t mask = mavlink_msg_set_position_target_local_ned_get_type_mask(msg);
    uint8_t frame = mavlink_msg_set_position_target_local_ned_get_coordinate_frame(msg);
    float vx = mavlink_msg_set_position_target_local_ned_get_vx(msg);
    float vy = mavlink_msg_set_position_target_local_ned_get_vy(msg);
    float vz = mavlink_msg_set_position_target_local_ned_get_vz(msg);
    float yaw_rate = mavlink_msg_set_position_target_local_ned_get_yaw_rate(msg);
    if (mask & POSITION_TARGET_TYPEMASK_VX_IGNORE) vx = 0;
    if (mask & POSITION_TARGET_TYPEMASK_VY_IGNORE) vy = 0;
    if (mask & POSITION_TARGET_TYPEMASK_VZ_IGNORE) vz = 0;
    if (mask & POSITION_TARGET_TYPEMASK_YAW_RATE_IGNORE) yaw_rate = 0;
    (void)vz; /* No vertical axis on this vehicle. */

    float bx = vx, by = vy;
    if (frame == MAV_FRAME_LOCAL_NED || frame == MAV_FRAME_LOCAL_OFFSET_NED) {
        float yaw = vf_yaw_ned(&m->fusion);
        float c = cosf(yaw), s = sinf(yaw);
        bx = vx * c + vy * s;
        by = -vx * s + vy * c;
    } else if (frame != MAV_FRAME_BODY_NED && frame != MAV_FRAME_BODY_OFFSET_NED &&
               frame != MAV_FRAME_BODY_FRD) {
        return; /* Unsupported frame: discard rather than guess. */
    }
    const float body[3] = { bx, by, -yaw_rate }; /* NED yaw rate is clockwise */
    vl_mav_motion(m->link, source_of(ch), body, VM_PI_TTL_MS, now_us);
}

static void handle_manual_control(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                                  uint64_t now_us) {
    uint32_t target = mavlink_msg_get_target_sysid(msg, mavlink_get_msg_entry(msg->msgid));
    if (target && target != VM_SYSTEM_ID) return;
    float x = clampf(mavlink_msg_manual_control_get_x(msg) / 1000.f, -1.f, 1.f);
    float y = clampf(mavlink_msg_manual_control_get_y(msg) / 1000.f, -1.f, 1.f);
    float r = clampf(mavlink_msg_manual_control_get_r(msg) / 1000.f, -1.f, 1.f);
    const float body[3] = { x * VM_RC_MAX_SPEED_MPS, y * VM_RC_MAX_SPEED_MPS,
                            -r * VM_RC_MAX_YAW_RATE };
    vl_mav_motion(m->link, source_of(ch), body, VM_RC_TTL_MS, now_us);
}

static float rc_axis(uint16_t raw) {
    float v = ((float)raw - 1500.f) / 500.f;
    v = clampf(v, -1.f, 1.f);
    return fabsf(v) < VM_RC_DEADBAND ? 0.f : v;
}

/* Local SBUS authority (opt-in). Every valid, non-failsafe frame renews the
 * 250 ms VL_RC lease, short enough to revoke motion before the 50 ms sensor
 * forwarding period could hide a lost receiver. Arm is a CH5 rising edge seen
 * low first; a switch stuck high at power-up never arms. Motion uses the same
 * stick map as the ESP32 bridge: CH1 lateral, CH2 forward, CH4 yaw. */
static void handle_local_sbus(VmContext *m, const VmRcSample *rc, uint32_t flags,
                              uint64_t now_us) {
#if VM_SBUS_AUTHORITY
    if (!rc->valid || (flags & VS_SBUS_FAILSAFE)) {
        m->rc_arm_low=0;
        vl_mav_link_lost(m->link,VL_RC,now_us);
        return;
    }
    if (m->rc_last_sample_us && rc->sample_us<=m->rc_last_sample_us) return;
    m->rc_last_sample_us=rc->sample_us;
    vl_rc_frame(m->link, rc->sample_us);
    unsigned index = VM_SBUS_ARM_CHANNEL - 1u;
    if (index >= 16u) return;
    int arm_switch = rc->channels[index] > VM_SBUS_ARM_THRESHOLD;
    if (!arm_switch) {
        m->rc_arm_low = 1;
        if (m->link->source == VL_RC &&
            (m->link->state == VL_ARMING || vl_state_active(m->link->state)))
            vl_mav_arm(m->link, VL_RC, 0, now_us);
        return;
    }
    if (!m->rc_arm_low && m->link->source!=VL_RC) return;
    if (!m->rc_arm_low && m->link->state==VL_DISARMED) return;
    m->rc_arm_low=0; /* Consume LOW-to-HIGH once, even if ARM is denied. */
    if (m->link->state == VL_DISARMED) vl_mav_arm(m->link, VL_RC, 1, now_us);
    /* Arming must not extend the lease beyond the physical receiver frame. */
    vl_rc_frame(m->link,rc->sample_us);
    const float body[3] = {
        rc_axis(rc->channels[1]) * VM_RC_MAX_SPEED_MPS,
        rc_axis(rc->channels[0]) * VM_RC_MAX_SPEED_MPS,
        -rc_axis(rc->channels[3]) * VM_RC_MAX_YAW_RATE
    };
    vl_mav_motion(m->link, VL_RC, body, VM_RC_TTL_MS, now_us);
#else
    (void)m; (void)rc; (void)flags; (void)now_us;
#endif
}

static void handle_rc_channels(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                               uint64_t now_us) {
    uint8_t count = mavlink_msg_rc_channels_get_chancount(msg);
    if (count > 16) count = 16;
    VmRcSample *rc = &m->sensors.rc;
    rc->count = count;
    rc->sample_us = now_us;
    rc->valid = count != 0;
    const uint16_t values[16] = {
        mavlink_msg_rc_channels_get_chan1_raw(msg),
        mavlink_msg_rc_channels_get_chan2_raw(msg),
        mavlink_msg_rc_channels_get_chan3_raw(msg),
        mavlink_msg_rc_channels_get_chan4_raw(msg),
        mavlink_msg_rc_channels_get_chan5_raw(msg),
        mavlink_msg_rc_channels_get_chan6_raw(msg),
        mavlink_msg_rc_channels_get_chan7_raw(msg),
        mavlink_msg_rc_channels_get_chan8_raw(msg),
        mavlink_msg_rc_channels_get_chan9_raw(msg),
        mavlink_msg_rc_channels_get_chan10_raw(msg),
        mavlink_msg_rc_channels_get_chan11_raw(msg),
        mavlink_msg_rc_channels_get_chan12_raw(msg),
        mavlink_msg_rc_channels_get_chan13_raw(msg),
        mavlink_msg_rc_channels_get_chan14_raw(msg),
        mavlink_msg_rc_channels_get_chan15_raw(msg),
        mavlink_msg_rc_channels_get_chan16_raw(msg)
    };
    memcpy(rc->channels, values, sizeof(values));
    if (count < 4) return;
    /* Aileron/elevator style mapping used by the ESP32 bridge: CH1 lateral,
     * CH2 forward, CH4 yaw. Arming is an explicit COMMAND_LONG. */
    const float body[3] = {
        rc_axis(rc->channels[1]) * VM_RC_MAX_SPEED_MPS,
        rc_axis(rc->channels[0]) * VM_RC_MAX_SPEED_MPS,
        -rc_axis(rc->channels[3]) * VM_RC_MAX_YAW_RATE
    };
    vl_mav_motion(m->link, source_of(ch), body, VM_RC_TTL_MS, now_us);
}

static void handle_param_set(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                             uint64_t now_us) {
    const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
    uint32_t target_system = mavlink_msg_get_target_sysid(msg, entry);
    uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
    if (target_system && target_system != VM_SYSTEM_ID) return;
    if (target_component && target_component != VM_COMPONENT_ID) return;
    char id[16] = {0};
    mavlink_msg_param_set_get_param_id(msg, id);
    int index = vm_param_index(id);
    if (index < 0) {
        status_text(m, ch, MAV_SEVERITY_WARNING, "Unknown param");
        return;
    }
    /* Calibration is only accepted while stopped; echo the current value when
     * the request is refused, which is how the parameter protocol reports it. */
    if (m->link->state == VL_DISARMED) {
        vm_param_set(&m->link->config, (unsigned)index,
                     mavlink_msg_param_set_get_param_value(msg));
        m->link->configured = (uint8_t)vc_config_valid(&m->link->config);
        m->link->config_revision++;
        /* Only a complete, valid image is worth persisting; a half-edited
         * calibration is refused by the encoder and would count as an error. */
        if (m->link->configured) vl_config_dirty(m->link, now_us);
    }
    send_param(m, ch, (unsigned)index);
}

static void handle_param_request_read(VmContext *m, unsigned ch,
                                      const mavlink_message_t *msg) {
    const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
    uint32_t target_system = mavlink_msg_get_target_sysid(msg, entry);
    uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
    if (target_system && target_system != VM_SYSTEM_ID) return;
    if (target_component && target_component != VM_COMPONENT_ID) return;
    int16_t index = mavlink_msg_param_request_read_get_param_index(msg);
    if (index >= 0) {
        if ((unsigned)index < VM_P_COUNT) send_param(m, ch, (unsigned)index);
        return;
    }
    char id[16] = {0};
    mavlink_msg_param_request_read_get_param_id(msg, id);
    int found = vm_param_index(id);
    if (found >= 0) send_param(m, ch, (unsigned)found);
}

static void handle_param_request_list(VmContext *m, unsigned ch,
                                      const mavlink_message_t *msg) {
    const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
    uint32_t target_system = mavlink_msg_get_target_sysid(msg, entry);
    uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
    if (target_system && target_system != VM_SYSTEM_ID) return;
    if (target_component && target_component != VM_COMPONENT_ID) return;
    for (unsigned i = 0; i < VM_P_COUNT; i++) send_param(m, ch, i);
}

static void handle_message(VmContext *m, unsigned ch, const mavlink_message_t *msg,
                           uint64_t now_us) {
    if (msg->sysid == VM_SYSTEM_ID && msg->compid == VM_COMPONENT_ID) return;
    switch (msg->msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:
        handle_heartbeat(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_COMMAND_LONG:
        handle_command_long(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED:
        handle_setpoint(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_MANUAL_CONTROL:
        handle_manual_control(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_RC_CHANNELS:
        handle_rc_channels(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_PARAM_SET:
        handle_param_set(m, ch, msg, now_us);
        break;
    case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
        handle_param_request_read(m, ch, msg);
        break;
    case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
        handle_param_request_list(m, ch, msg);
        break;
    case MAVLINK_MSG_ID_PING: {
        const mavlink_msg_entry_t *entry = mavlink_get_msg_entry(msg->msgid);
        uint32_t target = mavlink_msg_get_target_sysid(msg, entry);
        uint8_t target_component = mavlink_msg_get_target_compid(msg, entry);
        if (target && target != VM_SYSTEM_ID) return;
        if (target_component && target_component != VM_COMPONENT_ID) return;
        mavlink_message_t reply;
        mavlink_msg_ping_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &reply,
                                   mavlink_msg_ping_get_time_usec(msg),
                                   mavlink_msg_ping_get_seq(msg), msg->sysid, msg->compid);
        enqueue(m, ch, &reply);
        break;
    }
    case MAVLINK_MSG_ID_TIMESYNC: {
        uint32_t target = mavlink_msg_get_target_sysid(msg, mavlink_get_msg_entry(msg->msgid));
        if (target && target != VM_SYSTEM_ID) return;
        mavlink_message_t reply;
        mavlink_msg_timesync_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &reply,
                                       mavlink_msg_timesync_get_tc1(msg),
                                       (int64_t)(now_us * 1000u), msg->sysid, msg->compid);
        enqueue(m, ch, &reply);
        break;
    }
    default:
        break; /* Other MAVLink traffic is safely ignored. */
    }
}

void vm_rx(VmContext *m, unsigned channel, const uint8_t *data, size_t len, uint64_t now_us) {
    if (channel >= VM_CHANNELS || !data) return;
    mavlink_message_t msg;
    mavlink_status_t status;
    for (size_t i = 0; i < len; i++) {
        if (mavlink_parse_char((uint8_t)channel, data[i], &msg, &status)) {
            m->rx_messages[channel]++;
            handle_message(m, channel, &msg, now_us);
        }
    }
    m->rx_errors[channel] = mavlink_get_channel_status((uint8_t)channel)->parse_error;
}

void vm_link_lost(VmContext *m, unsigned channel, uint64_t now_us) {
    if (channel >= VM_CHANNELS) return;
    m->alive[channel] = 0;
    m->heartbeat_us[channel] = 0;
    vl_mav_link_lost(m->link, source_of(channel), now_us);
}

int vm_sensor(VmContext *m, uint8_t vp_type, const uint8_t *payload, uint16_t len,
              uint64_t now_us) {
    if (!payload || len < 8) return 0;
    switch (vp_type) {
    case VP_IMU:
        if (len != 48) return 0;
        m->sensors.imu.sample_us = vp_get_u64(payload);
        m->sensors.imu.sequence = vp_get_u32(payload + 36);
        for (unsigned i = 0; i < 3; i++) {
            m->sensors.imu.accel[i] = i32_at(payload + 8 + 4 * i) * 1e-6f;
            m->sensors.imu.gyro[i] = i32_at(payload + 20 + 4 * i) * 1e-6f;
        }
        m->sensors.imu.temperature_c = i32_at(payload + 32) * 1e-3f;
    m->fusion.temperature_c = m->sensors.imu.temperature_c;
        m->sensors.imu.pending = 1;
        return 1;
    case VP_GNSS: {
        if (len != 64) return 0;
        VmGnssSample *g = &m->sensors.gnss;
        g->observed_us = vp_get_u64(payload);
        g->utc_ms = vp_get_u64(payload + 8);
        g->lat = i32_at(payload + 16);
        g->lon = i32_at(payload + 20);
        g->alt_mm = i32_at(payload + 24);
        g->vel_n = i32_at(payload + 28);
        g->vel_e = i32_at(payload + 32);
        g->vel_d = i32_at(payload + 36);
        g->h_acc = i32_at(payload + 40);
        g->v_acc = i32_at(payload + 44);
        g->s_acc = i32_at(payload + 48);
        g->flags = vp_get_u32(payload + 52);
        g->fix_type = payload[56];
        g->satellites = payload[57];
        /* The VP payload carries the ellipsoid height only; no hMSL field. */
        g->hmsl_mm = g->alt_mm;
        g->valid = (uint8_t)((g->flags & 1u) != 0);
        return 1;
    }
    case VP_SBUS: {
        if (len != 48) return 0;
        VmRcSample *rc = &m->sensors.rc;
        uint32_t flags = vp_get_u32(payload + 44);
        rc->sample_us = vp_get_u64(payload);
        for (unsigned i = 0; i < 16; i++) rc->channels[i] = vp_get_u16(payload + 8 + 2 * i);
        rc->flags = flags;
        rc->valid = (uint8_t)((flags & 2u) != 0 && now_us - rc->sample_us < 200000u);
        rc->count = rc->valid ? 16 : 0;
        handle_local_sbus(m, rc, flags, now_us);
        return 1;
    }
    case VP_LIDAR: {
        unsigned count = payload[0];
        if (!count || count > 4 || len < 1 + 55 * count) return 0;
        VmScanSample *scan = &m->sensors.scan;
        for (unsigned f = 0; f < count; f++) {
            const uint8_t *raw = payload + 1 + 55 * f + 8;
            unsigned start = le16(raw + 4), end = le16(raw + 42);
            if (start >= 36000 || end >= 36000) continue;
            unsigned span = (end + 36000u - start) % 36000u;
            for (unsigned j = 0; j < 12; j++) {
                unsigned mm = le16(raw + 6 + 3 * j);
                if (!mm || mm > 12000) continue;
                unsigned angle = (start + span * j / 11u) % 36000u;
                unsigned bin = (angle / 100u) / 5u;
                if (bin >= VM_SCAN_BINS) continue;
                uint16_t cm = (uint16_t)(mm / 10u ? mm / 10u : 1u);
                if (!scan->distance_cm[bin] || cm < scan->distance_cm[bin])
                    scan->distance_cm[bin] = cm;
            }
        }
        scan->scan_us = now_us;
        scan->valid = 1;
        return 1;
    }
    default:
        return 0;
    }
}

static void fusion_update(VmContext *m, const VmInput *in, uint64_t now_us) {
    VmImuSample *imu = &m->sensors.imu;
    if (!imu->pending) return;
    imu->pending = 0;
    uint64_t stamp = imu->sample_us ? imu->sample_us : now_us;
    float accel[3], gyro[3];
    imu_map(imu->accel, accel, 0);
    imu_map(imu->gyro, gyro, 1);
    for (unsigned i = 0; i < 3; i++) accel[i] = -accel[i]; /* gravity direction, body FRD */
    if (m->last_imu_us && stamp > m->last_imu_us) {
        float dt = (float)(stamp - m->last_imu_us) * 1e-6f;
        /* vc_forward yaw is CCW-positive about z up; the filter gyro z is
         * positive clockwise about z down, so negate. */
        vf_set_wheel_rate(&m->fusion, -in->body_measured[2], in->configured);
        vf_update(&m->fusion, accel, gyro, dt);
    }
    m->last_imu_us = stamp;
}

static void odometry_update(VmContext *m, const VmInput *in, uint64_t now_us) {
    if (!m->last_odom_us) { m->last_odom_us = now_us; return; }
    float dt = (float)(now_us - m->last_odom_us) * 1e-6f;
    m->last_odom_us = now_us;
    if (!(dt > 0.f && dt < .2f)) return;
    float yaw = vf_yaw_ned(&m->fusion);
    float c = cosf(yaw), s = sinf(yaw);
    m->vel_n = in->body_measured[0] * c - in->body_measured[1] * s;
    m->vel_e = in->body_measured[0] * s + in->body_measured[1] * c;
    m->pos_n += m->vel_n * dt;
    m->pos_e += m->vel_e * dt;
}

static void check_links(VmContext *m, uint64_t now_us) {
    for (unsigned ch = 0; ch < VM_CHANNELS; ch++) {
        if (m->alive[ch] && now_us - m->heartbeat_us[ch] > VL_MAV_LINK_TIMEOUT_US) {
            m->alive[ch] = 0;
            vl_mav_link_lost(m->link, source_of(ch), now_us);
            if (m->alive[ch ^ 1u])
                status_text(m, ch ^ 1u, MAV_SEVERITY_WARNING,
                            ch == VM_CH_PI ? "PI link lost" : "ESP link lost");
        }
    }
    if (m->link->state != m->last_state) {
        const char *text = "STATE";
        switch (m->link->state) {
        case VL_ARMING: text = "ARMING"; break;
        case VL_RC_ACTIVE: text = "RC ARMED"; break;
        case VL_PI_ACTIVE: text = "PI ARMED"; break;
        case VL_WIFI_ACTIVE: text = "ESP ARMED"; break;
        case VL_DISARMED: text = "DISARMED"; break;
        case VL_FAILSAFE: text = "FAILSAFE"; break;
        case VL_FAULT: text = "FAULT"; break;
        default: break;
        }
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++)
            if (m->alive[ch]) status_text(m, ch, MAV_SEVERITY_WARNING, text);
        m->last_state = m->link->state;
    }
}

static int due(VmContext *m, unsigned index, uint64_t now_us) {
    if (now_us < m->next_us[index]) return 0;
    m->next_us[index] = now_us + vm_period[index];
    return 1;
}

static uint8_t armed(const VmInput *in) {
    return (uint8_t)(in->state == VL_ARMING || in->state == VL_RC_ACTIVE ||
                     in->state == VL_PI_ACTIVE || in->state == VL_WIFI_ACTIVE);
}

static void send_heartbeat(VmContext *m, unsigned ch, const VmInput *in) {
    uint8_t base = armed(in) ? MAV_MODE_FLAG_SAFETY_ARMED : 0;
    uint8_t system_status = MAV_STATE_STANDBY;
    if (in->state == VL_FAULT || in->state == VL_FAILSAFE) system_status = MAV_STATE_CRITICAL;
    else if (armed(in)) system_status = MAV_STATE_ACTIVE;
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                    MAV_TYPE_GROUND_ROVER, MAV_AUTOPILOT_GENERIC,
                                    base, 0, system_status);
    enqueue(m, ch, &msg);
}

static void send_status(VmContext *m, unsigned ch, const VmInput *in) {
    uint32_t present = MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL |
                       MAV_SYS_STATUS_SENSOR_GPS | MAV_SYS_STATUS_SENSOR_LASER_POSITION |
                       MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS | MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
    if (in->configured) present |= MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION;
    uint32_t health = MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL |
                      MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS;
    if (in->sensor_bits & 2u) health |= MAV_SYS_STATUS_SENSOR_GPS;
    if (in->sensor_bits & 4u) health |= MAV_SYS_STATUS_SENSOR_LASER_POSITION;
    if (m->sensors.rc.valid) health |= MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
    if (in->configured) health |= MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION;
    float load = 0.f;
    for (unsigned i = 0; i < 4; i++) load += fabsf(in->duty[i]);
    uint16_t load_permille = (uint16_t)clampf(load * 250.f, 0.f, 1000.f);
    uint16_t voltage = in->battery_valid ? in->vbat_mv : UINT16_MAX;
    mavlink_message_t msg;
    mavlink_msg_sys_status_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                     present, present, health, load_permille, voltage, -1, -1,
                                     0, (uint16_t)clampf((float)in->rx_errors, 0.f, 65535.f),
                                     (uint16_t)clampf((float)in->error_imu, 0.f, 65535.f),
                                     (uint16_t)clampf((float)in->error_gnss, 0.f, 65535.f),
                                     (uint16_t)clampf((float)in->control_overruns, 0.f, 65535.f),
                                     (uint16_t)clampf((float)in->error_uart, 0.f, 65535.f),
                                     0, 0, 0);
    enqueue(m, ch, &msg);
}

static void send_attitude(VmContext *m, unsigned ch) {
    float q[4], e[3];
    vf_quaternion(&m->fusion, q);
    (void)q;
    vf_euler(&m->fusion, e);
    mavlink_message_t msg;
    mavlink_msg_attitude_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                   (uint32_t)(m->last_odom_us / 1000u),
                                   e[0], e[1], e[2],
                                   m->fusion.gyro[0], m->fusion.gyro[1], m->fusion.gyro[2]);
    enqueue(m, ch, &msg);
}

static void send_imu(VmContext *m, unsigned ch, const VmInput *in) {
    (void)in;
    const VmImuSample *imu = &m->sensors.imu;
    mavlink_message_t msg;
    /* fusion.accel is the negated gravity direction used by the AHRS; report
     * the accelerometer itself (body FRD) to MAVLink consumers. */
    mavlink_msg_highres_imu_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                      imu->sample_us, -m->fusion.accel[0], -m->fusion.accel[1],
                                      -m->fusion.accel[2], m->fusion.gyro[0], m->fusion.gyro[1],
                                      m->fusion.gyro[2], NAN, NAN, NAN, 0, 0, 0,
                                      m->fusion.temperature_c, (uint16_t)0x103Fu, 0);
    enqueue(m, ch, &msg);
}

static void send_position(VmContext *m, unsigned ch) {
    mavlink_message_t msg;
    mavlink_msg_local_position_ned_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                             (uint32_t)(m->last_odom_us / 1000u),
                                             m->pos_n, m->pos_e, m->pos_d,
                                             m->vel_n, m->vel_e, 0.f);
    enqueue(m, ch, &msg);
}

static void send_odometry(VmContext *m, unsigned ch, const VmInput *in) {
    float q[4];
    const float covariance[21] = {0};
    vf_quaternion(&m->fusion, q);
    mavlink_message_t msg;
    mavlink_msg_odometry_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                   m->last_odom_us, MAV_FRAME_LOCAL_NED, MAV_FRAME_BODY_FRD,
                                   m->pos_n, m->pos_e, m->pos_d,
                                   q, in->body_measured[0], in->body_measured[1], 0.f,
                                   m->fusion.gyro[0], m->fusion.gyro[1], m->fusion.gyro[2],
                                   covariance, covariance, 0, MAV_ESTIMATOR_TYPE_UNKNOWN, -1);
    enqueue(m, ch, &msg);
}

static void send_rc(VmContext *m, unsigned ch) {
    const VmRcSample *rc = &m->sensors.rc;
    mavlink_message_t msg;
    mavlink_msg_rc_channels_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                      (uint32_t)(m->last_odom_us / 1000u),
                                      rc->valid ? rc->count : 0,
                                      rc->channels[0], rc->channels[1], rc->channels[2],
                                      rc->channels[3], rc->channels[4], rc->channels[5],
                                      rc->channels[6], rc->channels[7], rc->channels[8],
                                      rc->channels[9], rc->channels[10], rc->channels[11],
                                      rc->channels[12], rc->channels[13], rc->channels[14],
                                      rc->channels[15], 0, 0, 0);
    enqueue(m, ch, &msg);
}

static void send_battery(VmContext *m, unsigned ch, const VmInput *in) {
    uint16_t voltages[10];
    for (unsigned i = 0; i < 10; i++) voltages[i] = UINT16_MAX;
    voltages[0] = in->battery_valid ? in->vbat_mv : UINT16_MAX;
    /* battery_remaining is a linear per-cell voltage estimate, not a BMS
     * coulomb count. -1 keeps the field "unknown" when no voltage was read. */
    int8_t remaining = -1;
    if (in->soc_valid) remaining = (int8_t)clampf(in->soc * 100.f, 0.f, 100.f);
    const uint16_t ext[4] = {0, 0, 0, 0};
    mavlink_message_t msg;
    mavlink_msg_battery_status_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                         0, MAV_BATTERY_FUNCTION_ALL, MAV_BATTERY_TYPE_LIPO,
                                         INT16_MAX, voltages, -1, -1, -1, remaining, 0,
                                         MAV_BATTERY_CHARGE_STATE_UNDEFINED, ext, 0, 0);
    enqueue(m, ch, &msg);
}

static void send_gps(VmContext *m, unsigned ch, uint64_t now_us) {
    VmGnssSample *g = &m->sensors.gnss;
    int fresh = g->valid && g->observed_us && now_us - g->observed_us < 5000000u;
    uint8_t fix = 0;
    if (fresh) {
        if (g->fix_type >= 3) fix = 3;
        else if (g->fix_type == 2) fix = 2;
        else if (g->fix_type == 1) fix = 1;
    }
    uint16_t eph = UINT16_MAX, epv = UINT16_MAX, vel = UINT16_MAX;
    int16_t cog = 0;
    if (fresh) {
        eph = (uint16_t)(g->h_acc > 0 ? g->h_acc / 10 : 0);
        epv = (uint16_t)(g->v_acc > 0 ? g->v_acc / 10 : 0);
        float speed = sqrtf((float)g->vel_n * g->vel_n + (float)g->vel_e * g->vel_e);
        vel = (uint16_t)(speed / 10.f);
        float heading = atan2f((float)g->vel_e, (float)g->vel_n) * 57.2957795f;
        if (heading < 0) heading += 360.f;
        cog = (int16_t)(heading * 100.f);
    }
    uint64_t stamp = (fresh && (g->flags & 2u)) ? g->utc_ms * 1000u : 0;
    mavlink_message_t msg;
    mavlink_msg_gps_raw_int_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                      stamp, fix, fresh ? g->lat : 0, fresh ? g->lon : 0,
                                      fresh ? g->hmsl_mm : 0, eph, epv, vel, (uint16_t)cog,
                                      fresh ? g->satellites : 0,
                                      fresh ? g->alt_mm : 0,
                                      fresh && g->h_acc > 0 ? (uint32_t)g->h_acc : 0,
                                      fresh && g->v_acc > 0 ? (uint32_t)g->v_acc : 0,
                                      fresh && g->s_acc > 0 ? (uint32_t)g->s_acc : 0, 0, 0);
    enqueue(m, ch, &msg);
}

static void send_hud(VmContext *m, unsigned ch) {
    float speed = sqrtf(m->vel_n * m->vel_n + m->vel_e * m->vel_e);
    float heading = vf_yaw_ned(&m->fusion) * 57.2957795f;
    heading = fmodf(heading + 360.f, 360.f);
    mavlink_message_t msg;
    mavlink_msg_vfr_hud_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                  speed, speed, (int16_t)heading, 0, 0.f, 0.f);
    enqueue(m, ch, &msg);
}

static void send_scan(VmContext *m, unsigned ch) {
    mavlink_message_t msg;
    mavlink_msg_obstacle_distance_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                            m->sensors.scan.scan_us, MAV_DISTANCE_SENSOR_LASER,
                                            m->sensors.scan.distance_cm, 5, 1, 1200, 0.f, 0.f,
                                            MAV_FRAME_BODY_FRD);
    enqueue(m, ch, &msg);
}

static void send_named_one(VmContext *m, unsigned ch, const char *name, float value) {
    char field[10] = {0};
    strncpy(field, name, sizeof(field) - 1);
    mavlink_message_t msg;
    mavlink_msg_named_value_float_pack_chan(VM_SYSTEM_ID, VM_COMPONENT_ID, (uint8_t)ch, &msg,
                                            (uint32_t)(m->last_odom_us / 1000u),
                                            field, value);
    enqueue(m, ch, &msg);
}

static void send_named(VmContext *m, unsigned ch, const VmInput *in, uint64_t now_us) {
    send_named_one(m, ch, "vbat", in->battery_valid ? in->vbat_mv * .001f : NAN);
    send_named_one(m, ch, "vmot", in->battery_valid ? in->vmot_mv * .001f : NAN);
    send_named_one(m, ch, "vbody", in->body_measured[0]);
    send_named_one(m, ch, "wz", in->body_measured[2]);
    /* Link checks for the Pi diagnostics: heartbeat liveness and ages. */
    send_named_one(m, ch, "pi_up", m->alive[VM_CH_PI] ? 1.f : 0.f);
    send_named_one(m, ch, "esp_up", m->alive[VM_CH_ESP] ? 1.f : 0.f);
    send_named_one(m, ch, "esp_age", m->heartbeat_us[VM_CH_ESP] ?
                   (float)(now_us - m->heartbeat_us[VM_CH_ESP]) * 1e-6f : -1.f);
    send_named_one(m, ch, "rc_age", m->sensors.rc.sample_us ?
                   (float)(now_us - m->sensors.rc.sample_us) * 1e-6f : -1.f);
    send_named_one(m, ch, "state", (float)in->state);
    send_named_one(m, ch, "src", (float)in->source);
    send_named_one(m, ch, "fault", (float)in->fault_bits);
    send_named_one(m, ch, "cfg_rev", (float)m->link->config_revision);
    /* Electrical health. NAN marks "not measured" so a host never plots a
     * missing channel as a real zero. */
    send_named_one(m, ch, "i_fl", in->current_valid ? in->current_a[0] : NAN);
    send_named_one(m, ch, "i_fr", in->current_valid ? in->current_a[1] : NAN);
    send_named_one(m, ch, "i_rl", in->current_valid ? in->current_a[2] : NAN);
    send_named_one(m, ch, "i_rr", in->current_valid ? in->current_a[3] : NAN);
    send_named_one(m, ch, "ntc_c", in->ntc_valid ? in->ntc_c : NAN);
    send_named_one(m, ch, "soc", in->soc_valid ? in->soc : NAN);
    send_named_one(m, ch, "stall", (float)in->stall_mask);
    /* Runtime evidence: stack high-water in free words, loop jitter in us,
     * CPU load in percent, and persistence state. */
    send_named_one(m, ch, "stk_safe", (float)in->stack_free[0]);
    send_named_one(m, ch, "stk_ctrl", (float)in->stack_free[1]);
    send_named_one(m, ch, "stk_host", (float)in->stack_free[2]);
    send_named_one(m, ch, "stk_tele", (float)in->stack_free[3]);
    send_named_one(m, ch, "stk_pow", (float)in->stack_free[4]);
    send_named_one(m, ch, "stk_imu", (float)in->stack_free[5]);
    send_named_one(m, ch, "stk_rx", (float)in->stack_free[6]);
    send_named_one(m, ch, "jit_us", (float)in->jitter_last_us);
    send_named_one(m, ch, "jit_max", (float)in->jitter_max_us);
    send_named_one(m, ch, "cpu_pct", (float)in->cpu_percent);
    send_named_one(m, ch, "cfg_seq", (float)in->config_seq);
    send_named_one(m, ch, "cfg_err", (float)in->config_errors);
}

void vm_tick(VmContext *m, const VmInput *in, uint64_t now_us) {
    fusion_update(m, in, now_us);
    check_links(m, now_us);
    odometry_update(m, in, now_us);

    /* Heartbeat is emitted even before a peer is heard so it can discover us. */
    if (due(m, VM_R_HEARTBEAT, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) send_heartbeat(m, ch, in);

    /* Evaluate each schedule once, then fan out to every live peer. A dead
     * channel recovers quickly because the next due time keeps advancing. */
    if (due(m, VM_R_STATUS, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_status(m, ch, in);
    if (due(m, VM_R_ATTITUDE, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_attitude(m, ch);
    if (due(m, VM_R_IMU, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_imu(m, ch, in);
    if (due(m, VM_R_POSITION, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_position(m, ch);
    if (due(m, VM_R_ODOMETRY, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_odometry(m, ch, in);
    if (due(m, VM_R_RC, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_rc(m, ch);
    if (due(m, VM_R_BATTERY, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_battery(m, ch, in);
    if (due(m, VM_R_GPS, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_gps(m, ch, now_us);
    if (due(m, VM_R_HUD, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_hud(m, ch);
    if (due(m, VM_R_SCAN, now_us) && m->sensors.scan.valid &&
        now_us - m->sensors.scan.scan_us < 1000000u)
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_scan(m, ch);
    if (due(m, VM_R_NAMED, now_us))
        for (unsigned ch = 0; ch < VM_CHANNELS; ch++) if (m->alive[ch]) send_named(m, ch, in, now_us);
}
