#ifndef VEHICLE_SENSORS_H
#define VEHICLE_SENSORS_H
#include <stddef.h>
#include <stdint.h>

typedef struct { uint8_t data[47]; uint8_t used; uint32_t accepted, rejected; } VsLidar;
uint8_t vs_lidar_crc(const uint8_t *data, size_t len);
int vs_lidar_valid(const uint8_t data[47]);
int vs_lidar_feed(VsLidar *parser, uint8_t byte, uint8_t frame[47]);

#define VS_UBX_MAX 256u
typedef struct {
    uint8_t data[VS_UBX_MAX+8];
    uint8_t frame[VS_UBX_MAX+8];
    uint16_t used, expected;
    uint32_t accepted, rejected;
} VsUbx;
/* Returns 1 for complete valid UBX in parser->frame, including b5 62 sync.
 * Unknown bounded messages remain frame aligned. Output is valid until next feed. */
int vs_ubx_feed(VsUbx *parser, uint8_t byte);
size_t vs_ubx_encode(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *wire, size_t capacity);
int vs_gnss_payload(const uint8_t *nav_pvt, size_t len, uint64_t observed_us, uint8_t payload[64]);
size_t vs_gnss_config(uint8_t *wire, size_t capacity);
/* Burst TEMP_DATA1 through GYRO_DATA_Z0, bank0, big endian, ±4g/±500deg/s. */
int vs_imu_payload(const uint8_t data[14], uint64_t sample_us, uint32_t seq, uint32_t flags, uint8_t payload[48]);

/* SBUS1 and SBUS2 RC channel frames only; telemetry slots are not decoded. */
enum { VS_SBUS_SEEN=1u, VS_SBUS_FRESH=2u, VS_SBUS_LOST=4u,
       VS_SBUS_FAILSAFE=8u, VS_SBUS_TRANSPORT=16u };
typedef struct {
    uint8_t data[25], used, seen, transport_fault, flags;
    uint16_t channels[16];
    uint32_t accepted, rejected;
    uint64_t started_us, sample_us;
} VsSbus;
int vs_sbus_feed(VsSbus *parser, uint8_t byte, uint64_t observed_us);
void vs_sbus_lost(VsSbus *parser);
/* 48 bytes: observed sample u64, 16 raw u11 channels in u16, sequence u32,
 * status u32 (seen/fresh/lost/failsafe/transport, digital CH17/18 in bits8/9).
 * No mapping to motor commands or inferred PWM pulse widths. */
uint32_t vs_sbus_payload(const VsSbus *parser, uint64_t now_us, uint8_t payload[48]);
typedef struct { uint32_t stable_since_ms; uint8_t stable, enabled, fault_latched; } VsLidarPower;
/* Local load switch only. Require AUX PGOOD and RX ready for 50 ms;
 * either input falling revokes the enable on the next task step.
 * Branch protection faults latch until MCU restart; never auto-retry a short. */
int vs_lidar_power_step(VsLidarPower *power, uint32_t now_ms, int aux_good, int rx_ready, int branch_fault);
#endif
