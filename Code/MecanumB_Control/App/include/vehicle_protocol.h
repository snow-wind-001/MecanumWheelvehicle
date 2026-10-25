#ifndef VEHICLE_PROTOCOL_H
#define VEHICLE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define VP_VERSION 1u
#define VP_HEADER_SIZE 22u
#define VP_PAYLOAD_MAX 256u
#define VP_RAW_MAX 280u
#define VP_WIRE_MAX 283u

enum {
    VP_HELLO=0x01, VP_SYNC=0x02, VP_HEARTBEAT=0x10,
    VP_MOTION=0x11, VP_ARM=0x12, VP_LEASE=0x13,
    VP_ODOM=0x20, VP_IMU=0x21, VP_STATUS=0x22, VP_GNSS=0x23, VP_DIAGNOSTICS=0x24,
    VP_SBUS=0x25, VP_POWER_STATUS=0x26, VP_LIDAR=0x30, VP_PARAM=0x40, VP_POWER=0x41, VP_ACK=0x7f
};
enum { VP_RESPONSE=1u, VP_VALID=2u };
/* Optional STATUS offset30 extension. Without VALID, all GPIO fields are unknown. */
enum { VP_IO_PI_READY=1u, VP_IO_PI_HALTED=2u, VP_IO_PI_POWER_GOOD=4u,
       VP_IO_LEVEL_SHIFT_REQUESTED=8u, VP_IO_SHUTDOWN_REQUESTED=16u, VP_IO_VALID=0x8000u };

typedef struct {
    uint8_t type;
    uint16_t payload_len;
    uint32_t session, seq;
    uint64_t time_us;
    uint16_t flags;
    uint8_t payload[VP_PAYLOAD_MAX];
} VpFrame;

typedef struct {
    uint8_t encoded[VP_WIRE_MAX-1];
    uint16_t used;
    uint8_t dropping;
    uint32_t rejected, accepted;
} VpParser;

uint16_t vp_crc16(const uint8_t *data, size_t size);
size_t vp_encode(const VpFrame *frame, uint8_t *wire, size_t capacity);
/* Feed one wire byte. 1 = complete frame; 0 = pending/empty; -1 = rejected. */
int vp_feed(VpParser *parser, uint8_t byte, VpFrame *frame);
uint16_t vp_get_u16(const uint8_t *p);
uint32_t vp_get_u32(const uint8_t *p);
uint64_t vp_get_u64(const uint8_t *p);
void vp_put_u16(uint8_t *p, uint16_t v);
void vp_put_u32(uint8_t *p, uint32_t v);
void vp_put_u64(uint8_t *p, uint64_t v);
/* Half-range comparison: a distance of exactly 2^31 is rejected. */
int vp_seq_newer(uint32_t value, uint32_t previous);
#endif
