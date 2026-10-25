#include "vehicle_link.h"
#include <math.h>
#include <string.h>

#define LEASE_US UINT64_C(1000000)
static uint8_t active_mode(unsigned source) {
    return source==VL_PI ? VL_PI_ACTIVE : source==VL_RC ? VL_RC_ACTIVE : VL_WIFI_ACTIVE;
}
int vl_state_active(uint8_t state) {
    return state==VL_PI_ACTIVE || state==VL_WIFI_ACTIVE || state==VL_RC_ACTIVE;
}
static uint32_t token(VlLink *v) {
    /* Freshness counter, not cryptographic authentication. Transport must be trusted. */
    do { v->rng^=v->rng<<13; v->rng^=v->rng>>17; v->rng^=v->rng<<5; } while (!v->rng);
    return v->rng;
}
void vl_init(VlLink *v, uint32_t boot) {
    memset(v,0,sizeof(*v)); v->boot_id=boot; v->epoch=1;
    v->rng=boot ? boot : UINT32_C(0x6d656361); v->state=VL_DISARMED;
}
void vl_stop(VlLink *v, uint8_t state, uint32_t fault) {
    v->epoch++; v->state=state; v->fault_bits|=fault; v->arm_pending=0;
    v->command_until_us=0; memset(v->body,0,sizeof(v->body));
    for (unsigned i=0;i<VL_SOURCES;i++) { v->peer[i].token=0; v->peer[i].lease_until_us=0; }
}
int vl_permitted(const VlLink *v, uint64_t now) {
    return vl_state_active(v->state) &&
        (v->source!=VL_PI || v->pi_ready) &&
        (!v->mavlink || now<v->link_until_us[v->source]) &&
        !v->fault_bits && !v->power_inhibit && !v->maintenance && now<v->command_until_us && now<v->peer[v->source].lease_until_us;
}
void vl_mav_heartbeat(VlLink *v, unsigned source, uint64_t now) {
    if (source>=VL_SOURCES) return;
    v->link_until_us[source]=now+VL_MAV_LINK_TIMEOUT_US;
    v->peer[source].established=1;
    /* The lease is authorization, not motion: a heartbeat alone never keeps
     * the wheels turning because command_until_us still expires. */
    if (v->peer[source].lease_until_us<now+VL_MAV_LINK_TIMEOUT_US)
        v->peer[source].lease_until_us=now+VL_MAV_LINK_TIMEOUT_US;
}
void vl_mav_link_lost(VlLink *v, unsigned source, uint64_t now) {
    if (source>=VL_SOURCES) return;
    v->link_until_us[source]=0;
    if (v->source==source && (v->arm_pending || v->state==VL_ARMING ||
        vl_state_active(v->state))) vl_stop(v,VL_FAILSAFE,0);
    (void)now;
}
void vl_rc_frame(VlLink *v, uint64_t now) {
    uint64_t until=now+VL_RC_LINK_TIMEOUT_US;
    v->link_until_us[VL_RC]=until;
    v->peer[VL_RC].established=1;
    /* A receiver frame is both liveness and authorization. Do not extend it to
     * the 2 s companion lease: a lost SBUS link must revoke motion in 250 ms. */
    v->peer[VL_RC].lease_until_us=until;
}
void vl_config_dirty(VlLink *v, uint64_t now) {
    v->config_dirty=1;
    v->config_dirty_since_us=now;
}
int vl_mav_arm(VlLink *v, unsigned source, int arm, uint64_t now) {
    if (source>=VL_SOURCES) return VL_BAD_REQUEST;
    if (!arm) {
        /* Any companion may disarm; never leave an active source running. */
        if (v->source==source || v->state==VL_ARMING || vl_state_active(v->state) ||
            v->state==VL_FAILSAFE || v->state==VL_FAULT)
            vl_stop(v,VL_DISARMED,0);
        return VL_OK;
    }
    if (!v->mavlink || now>=v->link_until_us[source]) return VL_EXPIRED;
    /* A recoverable fault is cleared by an explicit arm attempt, but only when
     * no physical fault is still asserted. */
    if (v->state==VL_FAILSAFE || v->state==VL_FAULT) {
        if (v->physical_faults) return VL_DENIED;
        vl_stop(v,VL_DISARMED,0);
        v->state=VL_DISARMED; v->fault_bits=0;
    }
    if (!v->configured || v->fault_bits || v->physical_faults || v->power_inhibit || v->maintenance ||
        v->state!=VL_DISARMED || v->arm_pending || (source==VL_PI && !v->pi_ready))
        return VL_DENIED;
    v->source=(uint8_t)source; v->state=VL_ARMING; v->epoch++;
    v->arming_since_us=now; v->ready_since_us=now;
    /* 500 ms window matches vl_tick's arming timeout; promotion to ACTIVE needs
     * 20 ms of ready hardware and neutral wheels. */
    v->command_until_us=now+500000;
    /* The RC receiver is a 250 ms authority, not a 2 s companion lease. The
     * next accepted SBUS frame renews it; silence must revoke it. */
    v->peer[source].lease_until_us=now+(source==VL_RC ? VL_RC_LINK_TIMEOUT_US : VL_MAV_LINK_TIMEOUT_US);
    memset(v->body,0,sizeof(v->body));
    return VL_OK;
}
int vl_mav_motion(VlLink *v, unsigned source, const float body[3], uint32_t ttl_ms, uint64_t now) {
    if (source>=VL_SOURCES || !v->mavlink) return VL_BAD_REQUEST;
    if (now>=v->link_until_us[source]) return VL_EXPIRED;
    for (unsigned i=0;i<3;i++) if (!isfinite(body[i])) return VL_RANGE;
    if (v->state==VL_ARMING && v->source==source) {
        if (fabsf(body[0])+fabsf(body[1])+fabsf(body[2])>1e-4f) return VL_DENIED;
        return VL_OK;
    }
    if (v->source!=source || (v->state!=active_mode(source))) return VL_DENIED;
    if (fabsf(body[0])>3.f || fabsf(body[1])>3.f || fabsf(body[2])>10.f) return VL_RANGE;
    for (unsigned i=0;i<3;i++) v->body[i]=body[i];
    uint64_t deadline=now+(uint64_t)(ttl_ms ? ttl_ms : 500)*1000;
    if (deadline>v->peer[source].lease_until_us) deadline=v->peer[source].lease_until_us;
    v->command_until_us=deadline;
    v->last_command_us=now;
    return VL_OK;
}
void vl_power_update(VlLink *v, int inhibit) {
    if ((inhibit!=0)!=(v->power_inhibit!=0))
        vl_stop(v,v->fault_bits ? VL_FAULT : VL_DISARMED,0);
    v->power_inhibit=(uint8_t)(inhibit!=0);
    if (!inhibit) v->shutdown_accepted=0;
}
void vl_pi_inputs(VlLink *v, int power_good, int ready, int halted) {
    v->pi_ready=(uint8_t)(power_good && ready && !halted);
    if (!v->pi_ready && v->source==VL_PI &&
        (v->arm_pending || v->state==VL_ARMING || v->state==VL_PI_ACTIVE))
        vl_stop(v,VL_FAILSAFE,0);
}
void vl_tick(VlLink *v, uint64_t now, uint32_t faults, int ready, int neutral) {
    v->physical_faults=faults;
    if (faults) {
        if (v->state!=VL_FAULT || (faults&~v->fault_bits)) vl_stop(v,VL_FAULT,faults);
        return;
    }
    if (v->arm_pending && now>=v->peer[v->source].lease_until_us) vl_stop(v,VL_FAILSAFE,0);
    if (v->state==VL_ARMING) {
        if (now>=v->command_until_us || now>=v->peer[v->source].lease_until_us || now-v->arming_since_us>500000) {
            vl_stop(v,VL_FAILSAFE,0); return;
        }
        if (!ready || !neutral) v->ready_since_us=now;
        else if (now-v->ready_since_us>=20000) v->state=active_mode(v->source);
    } else if (vl_state_active(v->state)) {
        if (!ready) vl_stop(v,VL_FAULT,VL_FAULT_HARDWARE);
        else if (!vl_permitted(v,now)) vl_stop(v,VL_FAILSAFE,0);
    }
}

static uint32_t scaled(float value, float scale) {
    return value>0.f ? (uint32_t)(value*scale+.5f) : 0u;
}
int vl_config_encode(const VcConfig *c, uint8_t out[64]) {
    if (!c || !vc_config_valid(c)) return 0;
    memset(out,0,64);
    vp_put_u16(out,1); vp_put_u16(out+2,1);
    vp_put_u32(out+4,scaled(c->radius_m,1e6f));
    vp_put_u32(out+8,scaled(c->half_length_m,1e6f));
    vp_put_u32(out+12,scaled(c->half_width_m,1e6f));
    vp_put_u32(out+16,c->counts_per_rev);
    for (unsigned i=0;i<4;i++) { out[20+i]=(uint8_t)c->encoder_sign[i]; out[24+i]=(uint8_t)c->motor_sign[i]; }
    vp_put_u32(out+28,scaled(c->max_wheel_rad_s,1000.f));
    vp_put_u32(out+32,scaled(c->max_wheel_accel_rad_s2,1000.f));
    vp_put_u32(out+36,scaled(c->kp,1000.f));
    vp_put_u32(out+40,scaled(c->ki,1000.f));
    vp_put_u32(out+44,scaled(c->feedforward,1000.f));
    vp_put_u32(out+48,scaled(c->duty_limit,1000.f));
    vp_put_u32(out+52,scaled(c->speed_filter_s,1e6f));
    vp_put_u32(out+56,scaled(c->reversal_rad_s,1000.f));
    return 1;
}
int vl_config_decode(const uint8_t *p, size_t len, VcConfig *c) {
    if (len!=64 || vp_get_u16(p)!=1 || vp_get_u16(p+2)!=1 || vp_get_u32(p+60)) return 0;
    memset(c,0,sizeof(*c));
    c->radius_m=vp_get_u32(p+4)*1e-6f;
    c->half_length_m=vp_get_u32(p+8)*1e-6f; c->half_width_m=vp_get_u32(p+12)*1e-6f;
    c->counts_per_rev=vp_get_u32(p+16);
    for (unsigned i=0;i<4;i++) { c->encoder_sign[i]=(int8_t)p[20+i]; c->motor_sign[i]=(int8_t)p[24+i]; }
    c->max_wheel_rad_s=vp_get_u32(p+28)*.001f; c->max_wheel_accel_rad_s2=vp_get_u32(p+32)*.001f;
    c->kp=vp_get_u32(p+36)*.001f; c->ki=vp_get_u32(p+40)*.001f;
    c->feedforward=vp_get_u32(p+44)*.001f; c->duty_limit=vp_get_u32(p+48)*.001f;
    c->speed_filter_s=vp_get_u32(p+52)*1e-6f; c->reversal_rad_s=vp_get_u32(p+56)*.001f;
    return vc_config_valid(c);
}
static void ack(VpFrame *r, uint32_t seq, unsigned result) {
    r->type=VP_ACK; r->payload_len=8;
    vp_put_u32(r->payload,seq); vp_put_u16(r->payload+4,(uint16_t)result); vp_put_u16(r->payload+6,0);
}
static void lease(VlLink *v, VlPeer *p, VpFrame *r, uint64_t now) {
    p->token=token(v); p->lease_until_us=now+LEASE_US;
    r->type=VP_LEASE; r->payload_len=16;
    vp_put_u32(r->payload,p->token); vp_put_u32(r->payload+4,v->epoch);
    vp_put_u64(r->payload+8,p->lease_until_us);
}
void vl_packet(VlLink *v, unsigned src, const VpFrame *q, uint64_t now, VpFrame *r) {
    /* A late packet must not revive an expired target between safety ticks. */
    if ((v->state==VL_ARMING || vl_state_active(v->state)) &&
        (now>=v->command_until_us || now>=v->peer[v->source].lease_until_us)) vl_stop(v,VL_FAILSAFE,0);
    memset(r,0,sizeof(*r)); r->seq=++v->tx_seq; r->time_us=now; r->flags=VP_RESPONSE;
    r->session=q->session; ack(r,q->seq,VL_BAD_REQUEST);
    if (src>=VL_SOURCES || !q->session || q->flags || q->payload_len>VP_PAYLOAD_MAX) return;
    VlPeer *p=&v->peer[src];
    if (q->type==VP_HELLO) {
        if (q->payload_len!=0) return;
        if (p->established && p->session==q->session) {
            /* Repeated HELLO never resets the replay barrier or active control. */
            if (!vp_seq_newer(q->seq,p->last_seq)) { ack(r,q->seq,VL_SEQUENCE); return; }
        } else {
            if (src==v->source && (v->arm_pending || v->state==VL_ARMING || vl_permitted(v,now))) vl_stop(v,VL_DISARMED,0);
            memset(p,0,sizeof(*p)); p->session=q->session; p->established=1;
        }
        p->last_seq=q->seq; p->seq_valid=1;
        r->type=VP_HELLO; r->payload_len=12;
        vp_put_u32(r->payload,v->boot_id); vp_put_u32(r->payload+4,v->epoch);
        vp_put_u32(r->payload+8,1); /* capability bit 0: volatile calibration */
        return;
    }
    if (!p->established || p->session!=q->session) { ack(r,q->seq,VL_SESSION); return; }
    if (p->seq_valid && !vp_seq_newer(q->seq,p->last_seq)) { ack(r,q->seq,VL_SEQUENCE); return; }
    p->last_seq=q->seq; p->seq_valid=1;
    switch (q->type) {
    case VP_SYNC:
        if (q->payload_len!=8) return;
        r->type=VP_SYNC; r->payload_len=16;
        memcpy(r->payload,q->payload,8); vp_put_u64(r->payload+8,now); return;
    case VP_HEARTBEAT:
        if (q->payload_len) return;
        ack(r,q->seq,VL_OK); return; /* Never refresh motion or lease. */
    case VP_PARAM: {
        if (v->state!=VL_DISARMED || v->arm_pending) { ack(r,q->seq,VL_DENIED); return; }
        VcConfig c;
        if (!vl_config_decode(q->payload,q->payload_len,&c)) { ack(r,q->seq,VL_RANGE); return; }
        v->config=c; v->configured=1; v->config_revision++;
        vl_config_dirty(v,now); ack(r,q->seq,VL_OK); return;
    }
    case VP_ARM:
        if (q->payload_len!=4 || q->payload[2] || q->payload[3]) return;
        if (q->payload[0]==0) { vl_stop(v,VL_DISARMED,0); ack(r,q->seq,VL_OK); return; }
        if (q->payload[0]==2) {
            if (v->physical_faults) { ack(r,q->seq,VL_DENIED); return; }
            vl_stop(v,VL_DISARMED,0); v->fault_bits=0; ack(r,q->seq,VL_OK); return;
        }
        if (q->payload[0]!=1 || q->payload[1]!=active_mode(src)) return;
        if (!v->configured || v->fault_bits || v->physical_faults || v->power_inhibit || v->maintenance || v->state!=VL_DISARMED || v->arm_pending ||
            (src==VL_PI && !v->pi_ready)) {
            ack(r,q->seq,VL_DENIED); return;
        }
        v->source=(uint8_t)src; v->arm_pending=1; v->epoch++;
        memset(v->body,0,sizeof(v->body)); lease(v,p,r,now); return;
    case VP_POWER:
        /* v1 shutdown-only request. Caller must already have stopped/disarmed.
         * ACK means accepted, not Linux halted or power removed. */
        if (q->payload_len!=4 || q->payload[0]!=1 || q->payload[1]!=1 || q->payload[2] || q->payload[3]) return;
        if (src!=VL_PI || !v->pi_ready || v->power_inhibit || v->arm_pending ||
            (v->state!=VL_DISARMED && v->state!=VL_FAILSAFE && v->state!=VL_FAULT)) {
            ack(r,q->seq,VL_DENIED); return;
        }
        vl_power_update(v,1); v->shutdown_requested=1;
        ack(r,q->seq,VL_OK); return;
    case VP_LEASE:
        if (q->payload_len!=8) return;
        if (src!=v->source || !vl_permitted(v,now) || vp_get_u32(q->payload)!=p->token || vp_get_u32(q->payload+4)!=v->epoch) {
            ack(r,q->seq,VL_EXPIRED); return;
        }
        /* New token must be echoed in the next MOTION; old queued frames fail. */
        lease(v,p,r,now); return;
    case VP_MOTION: {
        if (v->maintenance) { ack(r,q->seq,VL_BUSY); return; }
        if (q->payload_len!=24 || q->payload[11] || q->payload[10]!=active_mode(src)) return;
        unsigned ttl=vp_get_u16(q->payload+8), max_ttl=src==VL_PI ? 200 : 250;
        if (!ttl || ttl>max_ttl) { ack(r,q->seq,VL_RANGE); return; }
        if (src!=v->source || !p->token || vp_get_u32(q->payload)!=p->token || vp_get_u32(q->payload+4)!=v->epoch || now>=p->lease_until_us) {
            ack(r,q->seq,VL_EXPIRED); return;
        }
        int32_t value[3];
        for (unsigned i=0;i<3;i++) value[i]=(int32_t)vp_get_u32(q->payload+12+4*i);
        if (value[0]<-3000 || value[0]>3000 || value[1]<-3000 || value[1]>3000 || value[2]<-10000 || value[2]>10000) {
            ack(r,q->seq,VL_RANGE); return;
        }
        if (v->arm_pending) {
            if (value[0] || value[1] || value[2]) { ack(r,q->seq,VL_DENIED); return; }
            v->arm_pending=0; v->state=VL_ARMING; v->arming_since_us=now; v->ready_since_us=now;
        } else if (v->state!=VL_ARMING && v->state!=active_mode(src)) { ack(r,q->seq,VL_DENIED); return; }
        if (v->state==VL_ARMING && (value[0] || value[1] || value[2])) { ack(r,q->seq,VL_DENIED); return; }
        for (unsigned i=0;i<3;i++) v->body[i]=value[i]*.001f;
        uint64_t deadline=now+(uint64_t)ttl*1000;
        v->command_until_us=deadline<p->lease_until_us ? deadline : p->lease_until_us;
        v->last_command_us=now; ack(r,q->seq,VL_OK); return;
    }
    default: return;
    }
}
