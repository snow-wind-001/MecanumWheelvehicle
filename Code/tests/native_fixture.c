/* Test-only ABI: drive the production MCU link over the production wire codec. */
#include "vehicle_link.h"
#include "vehicle_sensors.h"
#include "vehicle_power.h"
#include <stdlib.h>
#include <string.h>
typedef struct { VlLink link; VpParser parser; VsUbx ubx; VsLidar lidar; VsSbus sbus; VsLidarPower lidar_power; VwPower power; uint16_t power_inputs; uint32_t power_ms; uint8_t tx[32768]; size_t used; } Fixture;
void *fixture_create(void) { Fixture *f=calloc(1,sizeof(*f)); if (f) vl_init(&f->link,0x12345678); return f; }
void fixture_destroy(void *ctx) { free(ctx); }
void fixture_pi_inputs(void *ctx, int power_good, int ready, int halted) {
    vl_pi_inputs(&((Fixture *)ctx)->link,power_good,ready,halted);
}
void fixture_tick(void *ctx, uint64_t us, uint32_t faults, int ready) {
    Fixture *f=ctx; vl_tick(&f->link,us,faults,ready,1);
}
void fixture_feed(void *ctx, const uint8_t *data, size_t len, uint64_t us) {
    Fixture *f=ctx;
    for (size_t i=0;i<len;i++) {
        VpFrame request,response;
        if (vp_feed(&f->parser,data[i],&request)==1) {
            vl_packet(&f->link,VL_PI,&request,us,&response);
            if (f->used+VP_WIRE_MAX<=sizeof(f->tx)) f->used+=vp_encode(&response,f->tx+f->used,sizeof(f->tx)-f->used);
        }
    }
}
size_t fixture_read(void *ctx, uint8_t *data) { Fixture *f=ctx; size_t n=f->used; memcpy(data,f->tx,n); f->used=0; return n; }
unsigned fixture_state(void *ctx) { return ((Fixture *)ctx)->link.state; }
int fixture_permitted(void *ctx,uint64_t us) { return vl_permitted(&((Fixture *)ctx)->link,us); }
void fixture_status(void *ctx,uint64_t us) {
    Fixture *f=ctx; VpFrame r={.type=VP_STATUS,.payload_len=32,.time_us=us};
    r.session=f->link.peer[0].session; r.seq=++f->link.tx_seq;
    vp_put_u64(r.payload,us); vp_put_u32(r.payload+20,f->link.fault_bits); r.payload[24]=f->link.state;
    f->used+=vp_encode(&r,f->tx+f->used,sizeof(f->tx)-f->used);
}
void fixture_emit(void *ctx, uint8_t type, const uint8_t *payload, size_t len, uint64_t us) {
    Fixture *f=ctx; if (len>VP_PAYLOAD_MAX || f->used+VP_WIRE_MAX>sizeof(f->tx)) return;
    VpFrame r={.type=type,.payload_len=(uint16_t)len,.time_us=us};
    r.session=f->link.peer[0].session; r.seq=++f->link.tx_seq; memcpy(r.payload,payload,len);
    f->used+=vp_encode(&r,f->tx+f->used,sizeof(f->tx)-f->used);
}
void fixture_sensor_feed(void *ctx, unsigned sensor, const uint8_t *data, size_t len, uint64_t us) {
    Fixture *f=ctx; uint8_t payload[64];
    if (sensor==2) {
        if (len==14 && vs_imu_payload(data,us,1,0,payload)) fixture_emit(ctx,VP_IMU,payload,48,us);
        return;
    }
    for (size_t i=0;i<len;i++) {
        if (sensor==0 && vs_ubx_feed(&f->ubx,data[i])==1) {
            const uint8_t *frame=f->ubx.frame;
            if (frame[2]==1 && frame[3]==7 && vs_gnss_payload(frame+6,vp_get_u16(frame+4),us,payload)) fixture_emit(ctx,VP_GNSS,payload,64,us);
        } else if (sensor==3) {
            if (vs_sbus_feed(&f->sbus,data[i],us)==1) {
                vs_sbus_payload(&f->sbus,us,payload); fixture_emit(ctx,VP_SBUS,payload,48,us);
            }
        } else if (sensor==1) {
            uint8_t raw[47];
            if (vs_lidar_feed(&f->lidar,data[i],raw)==1) {
                payload[0]=1; vp_put_u64(payload+1,us); memcpy(payload+9,raw,47); fixture_emit(ctx,VP_LIDAR,payload,56,us);
            }
        }
    }
}
void fixture_sbus_report(void *ctx, uint64_t us, int transport_lost) {
    Fixture *f=ctx; uint8_t payload[48];
    if (transport_lost) vs_sbus_lost(&f->sbus);
    vs_sbus_payload(&f->sbus,us,payload); fixture_emit(ctx,VP_SBUS,payload,48,us);
}
int fixture_lidar_power(void *ctx, uint32_t ms, int aux_good, int rx_ready, int branch_fault) {
    return vs_lidar_power_step(&((Fixture *)ctx)->lidar_power,ms,aux_good,rx_ready,branch_fault);
}
unsigned fixture_power_step(void *ctx, uint32_t ms, int request, int good, int halted, int still, int off) {
    Fixture *f=ctx;
    request=request || f->link.shutdown_requested; f->link.shutdown_requested=0;
    vw_step(&f->power,ms,request,good,halted,still,off);
    vl_power_update(&f->link,f->power.inhibit);
    f->power_ms=ms; f->power_inputs=(good ? 8u : 0u)|(halted ? 16u : 0u)|(still ? 32u : 0u)|(off ? 64u : 0u);
    return f->power.state | (f->power.fault<<8) | (f->power.request_out<<16) |
           (f->power.cut_out<<17) | (f->power.inhibit<<18);
}
void fixture_power_report(void *ctx, uint64_t us) {
    Fixture *f=ctx; uint8_t payload[16]={0}; vp_put_u64(payload,us);
    payload[8]=f->power.state; payload[9]=f->power.fault;
    vp_put_u16(payload+10,f->power_inputs | (f->power.request_out ? 1u : 0u) |
               (f->power.cut_out ? 2u : 0u) | (f->link.power_inhibit ? 4u : 0u));
    vp_put_u32(payload+12,f->power_ms-f->power.stage_since_ms);
    fixture_emit(ctx,VP_POWER_STATUS,payload,sizeof(payload),us);
}
void fixture_power_applied(void *ctx, uint32_t ms, int previous_request, int previous_cut) {
    vw_applied(&((Fixture *)ctx)->power,ms,previous_request,previous_cut);
}
void fixture_mavlink(void *ctx) { ((Fixture *)ctx)->link.mavlink=1; }
void fixture_rc_frame(void *ctx, uint64_t us) { vl_rc_frame(&((Fixture *)ctx)->link,us); }
unsigned fixture_source(void *ctx) { return ((Fixture *)ctx)->link.source; }
unsigned fixture_faults(void *ctx) { return ((Fixture *)ctx)->link.fault_bits; }
int fixture_mav_arm(void *ctx, unsigned src, int arm, uint64_t us) {
    return vl_mav_arm(&((Fixture *)ctx)->link,src,arm,us);
}
int fixture_mav_motion(void *ctx, unsigned src, float x, float y, float z, uint32_t ttl, uint64_t us) {
    const float body[3]={x,y,z};
    return vl_mav_motion(&((Fixture *)ctx)->link,src,body,ttl,us);
}
void fixture_body(void *ctx, float out[3]) {
    memcpy(out,((Fixture *)ctx)->link.body,sizeof(((Fixture *)ctx)->link.body));
}
