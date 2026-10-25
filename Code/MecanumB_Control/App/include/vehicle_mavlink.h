#ifndef VEHICLE_MAVLINK_H
#define VEHICLE_MAVLINK_H

#include <stddef.h>
#include <stdint.h>
#include "vehicle_fusion.h"
#include "vehicle_link.h"

/* Standard MAVLink v2 (common dialect) companion adapter.
 *
 * Two physical links are served with independent MAVLink channels and
 * independent TX rings:
 *   channel 0 / USART6 : Raspberry Pi companion (ROS 2), primary authority
 *   channel 1 / USART1 : ESP32 radio bridge (RC input, telemetry relay)
 *
 * The module is pure C (no HAL, no RTOS). All entry points must be serialized
 * by the caller; the firmware wraps them in taskENTER_CRITICAL(). Telemetry
 * scheduling and the attitude/odometry estimator run in vm_tick(), which the
 * application calls from the 1 ms telemetry task.
 *
 * Authority always goes through vehicle_link.c: a heartbeat only proves that
 * a companion is alive, COMMAND_LONG(ARM_DISARM) starts the same ARMING path
 * as the legacy protocol, and every motion setpoint has a short deadline.
 * Losing heartbeats for VL_MAV_LINK_TIMEOUT_US revokes motion immediately.
 */

#ifndef VM_SYSTEM_ID
#define VM_SYSTEM_ID 1u
#endif
#ifndef VM_COMPONENT_ID
#define VM_COMPONENT_ID 1u /* MAV_COMP_ID_AUTOPILOT1 */
#endif

#define VM_CH_PI 0u
#define VM_CH_ESP 1u
#define VM_CHANNELS 2u

/* Ring holds several maximum-size frames; 921600 baud drains ~92 kB/s. */
#ifndef VM_TX_CAPACITY
#define VM_TX_CAPACITY 4096u
#endif

/* Setpoint deadlines. The Pi streams at >=10 Hz; the RC bridge at >=20 Hz. */
#ifndef VM_PI_TTL_MS
#define VM_PI_TTL_MS 500u
#endif
#ifndef VM_RC_TTL_MS
#define VM_RC_TTL_MS 250u
#endif

/* Manual/RC scaling. Authority limits in vehicle_link.c clamp harder. */
#ifndef VM_RC_MAX_SPEED_MPS
#define VM_RC_MAX_SPEED_MPS 1.0f
#endif
#ifndef VM_RC_MAX_YAW_RATE
#define VM_RC_MAX_YAW_RATE 2.0f
#endif
#ifndef VM_RC_DEADBAND
#define VM_RC_DEADBAND 0.06f
#endif

/* Local SBUS motion authority. The receiver is wired directly to the MCU, so
 * it keeps working when both companions are down. Off by default: the arm
 * channel and the receiver's failsafe channel values must be verified on the
 * bench first, because a wrong mapping would arm the vehicle. When enabled,
 * arm is a rising edge on CH5 after it has been seen low, and every valid
 * frame refreshes the 250 ms VL_RC lease. A frame marked failsafe never
 * refreshes it. */
#ifndef VM_SBUS_AUTHORITY
#define VM_SBUS_AUTHORITY 0
#endif
#ifndef VM_SBUS_ARM_CHANNEL
#define VM_SBUS_ARM_CHANNEL 5u /* 1-based; stored at channels[4] */
#endif
#ifndef VM_SBUS_ARM_THRESHOLD
#define VM_SBUS_ARM_THRESHOLD 1500u
#endif

/* Sensor-board axes relative to vehicle FRD (x forward, y right, z down).
 * The defaults assume the ICM-42688-P sits on the board's top side (sensor +z
 * up), so z is negated. Verify the permutation and every sign on the bench
 * before trusting attitude or odometry: these macros are the single place to
 * correct a rotated or flipped sensor. */
#ifndef VM_IMU_AXIS_X
#define VM_IMU_AXIS_X 0
#endif
#ifndef VM_IMU_AXIS_Y
#define VM_IMU_AXIS_Y 1
#endif
#ifndef VM_IMU_AXIS_Z
#define VM_IMU_AXIS_Z 2
#endif
#ifndef VM_IMU_SIGN_X
#define VM_IMU_SIGN_X 1.0f
#endif
#ifndef VM_IMU_SIGN_Y
#define VM_IMU_SIGN_Y 1.0f
#endif
#ifndef VM_IMU_SIGN_Z
#define VM_IMU_SIGN_Z -1.0f
#endif

/* Telemetry periods, microseconds. */
#ifndef VM_HEARTBEAT_PERIOD_US
#define VM_HEARTBEAT_PERIOD_US 1000000u
#endif
#ifndef VM_STATUS_PERIOD_US
#define VM_STATUS_PERIOD_US 1000000u
#endif
#ifndef VM_ATTITUDE_PERIOD_US
#define VM_ATTITUDE_PERIOD_US 100000u
#endif
#ifndef VM_IMU_PERIOD_US
#define VM_IMU_PERIOD_US 50000u
#endif
#ifndef VM_POSITION_PERIOD_US
#define VM_POSITION_PERIOD_US 100000u
#endif
#ifndef VM_ODOMETRY_PERIOD_US
#define VM_ODOMETRY_PERIOD_US 100000u
#endif
#ifndef VM_RC_PERIOD_US
#define VM_RC_PERIOD_US 100000u
#endif
#ifndef VM_BATTERY_PERIOD_US
#define VM_BATTERY_PERIOD_US 1000000u
#endif
#ifndef VM_GPS_PERIOD_US
#define VM_GPS_PERIOD_US 1000000u
#endif
#ifndef VM_HUD_PERIOD_US
#define VM_HUD_PERIOD_US 200000u
#endif
#ifndef VM_SCAN_PERIOD_US
#define VM_SCAN_PERIOD_US 200000u
#endif
#ifndef VM_NAMED_PERIOD_US
#define VM_NAMED_PERIOD_US 1000000u
#endif

/* Latest sensor samples, written by vm_sensor() and consumed by vm_tick(). */
typedef struct {
    float accel[3], gyro[3], temperature_c;
    uint64_t sample_us;
    uint32_t sequence;
    uint8_t pending;
} VmImuSample;

typedef struct {
    uint64_t observed_us, utc_ms;
    int32_t lat, lon;      /* 1e-7 deg */
    int32_t alt_mm;        /* ellipsoid */
    int32_t hmsl_mm;
    int32_t vel_n, vel_e, vel_d; /* mm/s */
    int32_t h_acc, v_acc, s_acc; /* mm, mm, mm/s */
    uint8_t fix_type, satellites;
    uint32_t flags;
    uint8_t valid;
} VmGnssSample;

typedef struct {
    uint16_t channels[16];
    uint64_t sample_us;
    uint32_t flags;
    uint8_t count;
    uint8_t valid;
} VmRcSample;

#define VM_SCAN_BINS 72u
typedef struct {
    uint16_t distance_cm[VM_SCAN_BINS];
    uint64_t scan_us;
    uint8_t valid;
} VmScanSample;

typedef struct {
    VmImuSample imu;
    VmGnssSample gnss;
    VmRcSample rc;
    VmScanSample scan;
} VmSensors;

/* Vehicle state sampled by the application for one vm_tick(). */
typedef struct {
    float body_measured[3]; /* forward m/s, right m/s, yaw CCW rad/s */
    float wheel_measured[4];
    float duty[4];
    uint16_t vbat_mv, vmot_mv;
    uint8_t battery_valid;
    uint8_t state, source, configured, pi_ready, power_inhibit;
    uint32_t fault_bits;
    uint32_t sensor_bits; /* vehicle_sensors_diagnostics()[0] */
    uint32_t error_imu, error_gnss, error_lidar, error_uart;
    uint32_t rx_errors, control_overruns, tx_dropped;

    /* Electrical health. Invalid values are encoded as NAN / -1, never as a
     * plausible-looking zero. */
    float current_a[4];
    float ntc_c;
    float soc;             /* 0..1, voltage-based estimate, not a BMS */
    uint8_t current_valid, ntc_valid, soc_valid;
    uint32_t stall_mask;   /* bit i: wheel i stalled for VH_STALL_MS */

    /* Runtime evidence sampled on the vehicle, not host-side estimates. */
    uint16_t stack_free[7]; /* Safety, Control, HostRx, Telemetry, Power, Imu, SensorRx */
    uint32_t jitter_last_us, jitter_max_us;
    uint8_t cpu_percent;
    uint32_t config_seq, config_errors;
} VmInput;

typedef struct {
    VlLink *link;
    VmSensors sensors;
    VfFusion fusion;

    float pos_n, pos_e, pos_d;
    float vel_n, vel_e;
    uint64_t last_odom_us;
    uint64_t last_imu_us;

    uint64_t heartbeat_us[VM_CHANNELS];
    uint8_t alive[VM_CHANNELS];
    uint8_t remote_type[VM_CHANNELS];
    uint8_t last_state;
    uint32_t status_seq;
    /* Local SBUS arm switch: CH5 must be seen low before a rising edge counts,
     * so a switch left high at power-up cannot arm the vehicle. */
    uint8_t rc_arm_low;
    uint64_t rc_last_sample_us;

    uint32_t rx_messages[VM_CHANNELS];
    uint32_t rx_errors[VM_CHANNELS];
    uint32_t tx_overflow[VM_CHANNELS];

    uint64_t next_us[12];
    uint8_t tx_buf[VM_CHANNELS][VM_TX_CAPACITY];
    uint32_t tx_head[VM_CHANNELS], tx_tail[VM_CHANNELS];
} VmContext;

void vm_init(VmContext *m, VlLink *link, uint64_t now_us);

/* Feed received bytes for one physical channel. Parses MAVLink frames,
 * renews liveness on HEARTBEAT, and handles arm/motion/PING/TIMESYNC.
 * Replies are queued in the same channel's TX ring. */
void vm_rx(VmContext *m, unsigned channel, const uint8_t *data, size_t len, uint64_t now_us);

/* Mark a transport failure (UART DMA restart, framing gap). Revokes authority
 * for the active source and forgets the heartbeat. */
void vm_link_lost(VmContext *m, unsigned channel, uint64_t now_us);

/* Consume sensor payloads produced by the existing sensor tasks (VP_IMU,
 * VP_GNSS, VP_SBUS, VP_LIDAR). Returns 1 when the type was accepted. */
int vm_sensor(VmContext *m, uint8_t vp_type, const uint8_t *payload, uint16_t len, uint64_t now_us);

/* Run the estimator and emit due telemetry for every live channel. */
void vm_tick(VmContext *m, const VmInput *in, uint64_t now_us);

/* Pull up to capacity bytes from a channel's serialized TX stream. Returns the
 * number of bytes copied. The caller owns the UART and its busy flag. */
uint16_t vm_tx_pop(VmContext *m, unsigned channel, uint8_t *dst, uint16_t capacity);

/* Account a failed/aborted DMA transfer. */
void vm_tx_drop(VmContext *m, unsigned channel);

int vm_alive(const VmContext *m, unsigned channel);

#endif
