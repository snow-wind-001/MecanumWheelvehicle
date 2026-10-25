#include "vehicle_tx.h"

static int older(uint32_t a, uint32_t b) { return a!=b && (uint32_t)(b-a)<UINT32_C(0x80000000); }
int vt_push(VtPool *p, unsigned port, unsigned priority, const VpFrame *f) {
    if (port>1 || priority>VT_SCAN || f->payload_len>VP_PAYLOAD_MAX) return 0;
    int slot=-1, victim=-1; unsigned scan_count=0;
    for (unsigned i=0;i<VT_BLOCKS;i++) {
        VtBlock *b=&p->blocks[i];
        if (!b->used) { if (slot<0) slot=(int)i; continue; }
        if (priority==VT_PERIODIC && b->priority==priority && b->port==port && b->frame.type==f->type) {
            b->frame=*f; b->order=++p->order; p->coalesced++; return 1;
        }
        if (b->priority==VT_SCAN && b->port==port) scan_count++;
        if (b->priority>priority && (victim<0 || b->priority>p->blocks[victim].priority ||
            (b->priority==p->blocks[victim].priority && older(b->order,p->blocks[victim].order)))) victim=(int)i;
    }
    if (priority==VT_SCAN && scan_count>=8) {
        /* Retain recent scan segments under congestion. Lost bins remain unknown. */
        for (unsigned i=0;i<VT_BLOCKS;i++) {
            VtBlock *b=&p->blocks[i];
            if (b->used && b->port==port && b->priority==VT_SCAN && (victim<0 || older(b->order,p->blocks[victim].order))) victim=(int)i;
        }
        slot=victim;
        p->dropped++;
    } else if (slot<0) {
        if (victim<0) { p->dropped++; return 0; }
        slot=victim; p->dropped++;
    }
    VtBlock *b=&p->blocks[slot]; b->frame=*f; b->priority=(uint8_t)priority; b->port=(uint8_t)port;
    b->order=++p->order; b->used=1; return 1;
}
int vt_pop(VtPool *p, unsigned port, VpFrame *f) {
    int best=-1;
    for (unsigned i=0;i<VT_BLOCKS;i++) {
        VtBlock *b=&p->blocks[i];
        if (!b->used || b->port!=port) continue;
        if (best<0 || b->priority<p->blocks[best].priority ||
            (b->priority==p->blocks[best].priority && older(b->order,p->blocks[best].order))) best=(int)i;
    }
    if (best<0) return 0;
    *f=p->blocks[best].frame; p->blocks[best].used=0; return 1;
}
