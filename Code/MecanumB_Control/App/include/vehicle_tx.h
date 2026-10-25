#ifndef VEHICLE_TX_H
#define VEHICLE_TX_H
#include "vehicle_protocol.h"
#define VT_BLOCKS 32
enum { VT_CONTROL, VT_PERIODIC, VT_SCAN };
typedef struct { VpFrame frame; uint32_t order; uint8_t used, port, priority; } VtBlock;
typedef struct { VtBlock blocks[VT_BLOCKS]; uint32_t order, dropped, coalesced; } VtPool;
/* All pool access is serialized by the application. ISR never owns pool blocks. */
int vt_push(VtPool *pool, unsigned port, unsigned priority, const VpFrame *frame);
int vt_pop(VtPool *pool, unsigned port, VpFrame *frame);
#endif
