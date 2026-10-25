#include "vehicle_store.h"
#include "vehicle_link.h"
#include "vehicle_protocol.h"
#include <string.h>

uint32_t vs_crc32(const void *data, uint32_t len) {
    const uint8_t *p=data;
    uint32_t crc=UINT32_C(0xffffffff);
    for (uint32_t i=0;i<len;i++) {
        crc^=p[i];
        for (unsigned bit=0;bit<8;bit++) crc=(crc>>1)^(UINT32_C(0xedb88320)&(uint32_t)-(int32_t)(crc&1u));
    }
    return ~crc;
}

static int slot_scan(const VsSlot *s, uint32_t *next, uint32_t *best_seq, VcConfig *best, int *found) {
    uint32_t offset=0, newest=0;
    int have=0;
    while (offset+VS_RECORD_BYTES<=s->size) {
        uint8_t record[VS_RECORD_BYTES];
        if (!s->read || !s->read(s->address+offset,record,sizeof(record),s->ctx)) break;
        int blank=1;
        for (unsigned i=0;i<sizeof(record);i++) if (record[i]!=0xffu) { blank=0; break; }
        if (blank) break;
        /* A torn header consumes its fixed-size slot. Never retry programming
         * non-erased flash or hide later complete records behind a bad magic. */
        if (vp_get_u32(record)!=VS_RECORD_MAGIC) { offset+=VS_RECORD_BYTES; continue; }
        if (vp_get_u16(record+4)==VS_RECORD_VERSION && vp_get_u16(record+6)==VS_CONFIG_BYTES) {
            uint32_t seq=vp_get_u32(record+8), stored_crc=vp_get_u32(record+12);
            uint8_t check[VS_RECORD_BYTES];
            memcpy(check,record,sizeof(check));
            vp_put_u32(check+12,0);
            if (vs_crc32(check,sizeof(check))==stored_crc) {
                VcConfig candidate;
                if (vl_config_decode(record+20,VS_CONFIG_BYTES,&candidate) &&
                    (!have || seq>newest)) {
                    newest=seq; *best=candidate; have=1;
                }
            }
        }
        offset+=VS_RECORD_BYTES;
    }
    *next=offset; *best_seq=newest; *found=have;
    return 1;
}

int vs_store_init(VsStore *s, const VsSlot slots[2], int allow_erase, VcConfig *out) {
    if (!s || !slots || !out) return 0;
    memset(s,0,sizeof(*s));
    s->slot[0]=slots[0]; s->slot[1]=slots[1];
    uint32_t next[2]={0,0}, seq[2]={0,0};
    VcConfig config[2];
    int found[2]={0,0};
    for (unsigned i=0;i<2;i++)
        slot_scan(&s->slot[i],&next[i],&seq[i],&config[i],&found[i]);
    unsigned active=0;
    if (found[1] && (!found[0] || seq[1]>seq[0])) active=1;
    int any=found[0] || found[1];
    s->seq=seq[active];
    if (any && out) *out=config[active];
    s->active=active;
    s->next_offset=next[active];
    s->writable=1;

    if (!any) {
        /* A partially written or foreign image in slot 0 may not be erasable
         * word-by-word. Boot-time erase is the only safe place to recover. */
        if (allow_erase && next[0]!=0) {
            if (!s->slot[0].erase || !s->slot[0].erase(s->slot[0].address,s->slot[0].size,s->slot[0].ctx))
                s->writable=0;
        } else if (!allow_erase) {
            s->writable=0;
        }
        s->next_offset=0;
        return 0;
    }

    if (s->next_offset+VS_RECORD_BYTES>s->slot[active].size) {
        unsigned other=active^1u;
        if (allow_erase && s->slot[other].erase &&
            s->slot[other].erase(s->slot[other].address,s->slot[other].size,s->slot[other].ctx)) {
            s->active=other; s->next_offset=0;
        } else {
            s->writable=0;
        }
    }
    return 1;
}

int vs_store_save(VsStore *s, const VcConfig *config, uint32_t boot_id) {
    if (!s || !config) return 0;
    VsSlot *slot=&s->slot[s->active];
    if (!s->writable || s->next_offset+VS_RECORD_BYTES>slot->size) {
        s->writable=0; s->errors++; return 0;
    }
    uint8_t encoded[VS_CONFIG_BYTES];
    if (!vl_config_encode(config,encoded)) { s->errors++; return 0; }

    uint8_t record[VS_RECORD_BYTES] __attribute__((aligned(4)));
    memset(record,0,sizeof(record));
    vp_put_u32(record,VS_RECORD_MAGIC);
    vp_put_u16(record+4,VS_RECORD_VERSION);
    vp_put_u16(record+6,VS_CONFIG_BYTES);
    vp_put_u32(record+8,s->seq+1);
    vp_put_u32(record+16,boot_id);
    memcpy(record+20,encoded,VS_CONFIG_BYTES);
    vp_put_u32(record+12,vs_crc32(record,sizeof(record)));

    uint32_t address=slot->address+s->next_offset;
    if (!slot->program || !slot->program(address,record,sizeof(record),slot->ctx)) {
        s->writable=0; s->errors++; return 0;
    }
    uint8_t verify[VS_RECORD_BYTES];
    if (!slot->read || !slot->read(address,verify,sizeof(verify),slot->ctx) ||
        memcmp(verify,record,sizeof(record))!=0) {
        s->writable=0; s->errors++; return 0;
    }
    s->seq++;
    s->next_offset+=VS_RECORD_BYTES;
    if (s->next_offset+VS_RECORD_BYTES>slot->size) s->writable=0;
    return 1;
}

uint32_t vs_store_seq(const VsStore *s) { return s ? s->seq : 0; }
uint32_t vs_store_errors(const VsStore *s) { return s ? s->errors : 0; }
