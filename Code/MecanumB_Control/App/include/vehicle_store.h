#ifndef VEHICLE_STORE_H
#define VEHICLE_STORE_H
#include <stdint.h>
#include "vehicle_control.h"

/* A/B parameter log in two flash sectors.
 *
 * Each record stores the same 64-byte v1 PARAM image that the host sends over
 * VP/MAVLink, so persisted data is versioned, endian-defined, and validated by
 * vc_config_valid() on load. Records are appended; a sector is erased only
 * during boot, before the independent watchdog is started. At runtime a full
 * sector stops accepting writes and is rotated on the next boot.
 *
 * The module is pure C: the caller supplies erase/program/read callbacks, so
 * native tests use a RAM-backed flash and the firmware uses HAL word programming and an SRAM boot erase/feed loop.
 */

#define VS_RECORD_MAGIC   UINT32_C(0x4d435631) /* "MCV1" */
#define VS_RECORD_VERSION 1u
#define VS_CONFIG_BYTES   64u
#define VS_RECORD_BYTES   84u

typedef struct {
    uint32_t address;
    uint32_t size;
    int (*erase)(uint32_t address, uint32_t size, void *ctx);
    int (*program)(uint32_t address, const void *data, uint32_t len, void *ctx);
    int (*read)(uint32_t address, void *dst, uint32_t len, void *ctx);
    void *ctx;
} VsSlot;

typedef struct {
    VsSlot slot[2];
    uint32_t active;       /* slot used for the next append */
    uint32_t next_offset;  /* append offset inside active */
    uint32_t seq;          /* newest sequence number seen or written */
    uint32_t writable;     /* space remains in the active slot */
    uint32_t errors;
} VsStore;

/* Scan both slots, load the newest valid configuration, and prepare the
 * append point. When the active slot is full and allow_erase is nonzero, the
 * older slot is erased and becomes active. Call this during boot before the
 * watchdog starts. Returns 1 when a valid record was loaded. */
int vs_store_init(VsStore *s, const VsSlot slots[2], int allow_erase, VcConfig *out);

/* Append one record. Never erases. Returns 1 only after read-back CRC. */
int vs_store_save(VsStore *s, const VcConfig *config, uint32_t boot_id);

uint32_t vs_store_seq(const VsStore *s);
uint32_t vs_store_errors(const VsStore *s);
uint32_t vs_crc32(const void *data, uint32_t len);

#endif
