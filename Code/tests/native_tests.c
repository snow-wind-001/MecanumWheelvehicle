#include "vehicle_protocol.h"
#include "vehicle_control.h"
#include "vehicle_link.h"
#include "vehicle_sensors.h"
#include "vehicle_tx.h"
#include "vehicle_power.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static VcConfig config(void) {
    VcConfig c={.radius_m=.05f,.half_length_m=.15f,.half_width_m=.12f,.counts_per_rev=2048,
        .encoder_sign={1,1,1,1},.motor_sign={1,1,1,1},.max_wheel_rad_s=20,
        .max_wheel_accel_rad_s2=100,.kp=.1f,.ki=.2f,.feedforward=.02f,
        .duty_limit=.5f,.speed_filter_s=.01f,.reversal_rad_s=.5f}; return c;
}
static void protocol_test(void) {
    assert(vp_crc16((const uint8_t *)"123456789",9)==0x29b1);
    assert(vp_seq_newer(0,UINT32_MAX)); assert(!vp_seq_newer(0,0)); assert(!vp_seq_newer(0x80000000,0));
    uint32_t seed=93;
    for (unsigned length=0;length<=256;length++) {
        VpFrame f={.type=0x11,.payload_len=length,.session=0xfedcba98,.seq=UINT32_MAX,.time_us=UINT64_C(0xfedcba9876543210),.flags=2}, out;
        for (unsigned i=0;i<length;i++) { seed=seed*1664525+1013904223; f.payload[i]=(uint8_t)(seed>>24); }
        uint8_t wire[VP_WIRE_MAX]; size_t n=vp_encode(&f,wire,sizeof(wire)); assert(n && n<=VP_WIRE_MAX);
        VpParser p={0};
        for (size_t i=0;i<n;i++) assert(vp_feed(&p,wire[i],&out)==(i==n-1 ? 1 : 0));
        assert(out.type==f.type && out.payload_len==length && out.session==f.session && out.seq==f.seq && out.time_us==f.time_us && out.flags==2);
        assert(!memcmp(f.payload,out.payload,length)); assert(vp_encode(&f,wire,n-1)==0);
        for (unsigned i=0;i<1000;i++) vp_feed(&p,1,&out);
        assert(vp_feed(&p,0,&out)==-1);
        int got=0; for (size_t i=0;i<n;i++) got=vp_feed(&p,wire[i],&out); assert(got==1);
        uint32_t accepted=p.accepted, rejected=p.rejected;
        wire[n-2]^=0x80; for (size_t i=0;i<n;i++) vp_feed(&p,wire[i],&out);
        assert(p.accepted==accepted && p.rejected>rejected);
    }
}
static void control_test(void) {
    VcConfig c=config(); assert(vc_config_valid(&c));
    float w[4],body[3]; vc_inverse(&c,.2f,-.1f,.3f,w); vc_forward(&c,w,body);
    assert(fabsf(body[0]-.2f)<1e-5f && fabsf(body[1]+.1f)<1e-5f && fabsf(body[2]-.3f)<1e-5f);
    vc_inverse(&c,10,3,8,w); float largest=0; for (unsigned i=0;i<4;i++) largest=fmaxf(largest,fabsf(w[i])); assert(fabsf(largest-20)<1e-5f);
    assert(vc_encoder_delta(2,65534,16)==4); assert(vc_encoder_delta(65534,2,16)==-4);
    assert(vc_encoder_delta(2,UINT32_MAX-1,32)==4); assert(vc_encoder_delta(UINT32_MAX-1,2,32)==-4);
    VcControl v; vc_init(&v,&c); uint32_t counts[4]={0}; float target[3]={.5f,0,0};
    for (unsigned i=0;i<5000;i++) vc_step(&v,counts,target,.001f,1);
    for (unsigned i=0;i<4;i++) { assert(v.duty[i]<=.5f && v.duty[i]>0); assert(fabsf(v.integral[i])<=.5f); }
    vc_step(&v,counts,target,.001f,0); for (unsigned i=0;i<4;i++) assert(v.duty[i]==0 && v.integral[i]==0);
    vc_step(&v,counts,target,NAN,1); for (unsigned i=0;i<4;i++) assert(v.duty[i]==0);
    c.counts_per_rev=0; vc_init(&v,&c); vc_step(&v,counts,target,.001f,1); assert(!v.configured && v.duty[0]==0);
}
static void link_test(void) {
    VlLink v; vl_init(&v,1234); VpFrame q={.session=22,.type=VP_HELLO,.seq=1},r;
    vl_packet(&v,VL_PI,&q,1000,&r); assert(r.type==VP_HELLO);
    q.type=VP_ARM; q.payload_len=4; q.payload[0]=1; q.payload[1]=VL_PI_ACTIVE; q.seq++;
    vl_packet(&v,VL_PI,&q,2000,&r); assert(r.type==VP_ACK && vp_get_u16(r.payload+4)==VL_DENIED);
    v.config=config(); v.configured=1;
    q.seq++; vl_packet(&v,VL_PI,&q,2500,&r); assert(r.type==VP_ACK && vp_get_u16(r.payload+4)==VL_DENIED);
    vl_pi_inputs(&v,1,1,0);
    q.seq++; vl_packet(&v,VL_PI,&q,3000,&r); assert(r.type==VP_LEASE && v.arm_pending);
    uint32_t old_token=v.peer[0].token;
    q.type=VP_MOTION; q.payload_len=24; memset(q.payload,0,24);
    vp_put_u32(q.payload,old_token); vp_put_u32(q.payload+4,v.epoch); vp_put_u16(q.payload+8,100); q.payload[10]=VL_PI_ACTIVE;
    q.seq++; vl_packet(&v,0,&q,4000,&r); assert(v.state==VL_ARMING);
    vl_tick(&v,25000,0,1,1); assert(v.state==VL_PI_ACTIVE && vl_permitted(&v,25000));
    /* Duplicate sequence and old token cannot update the deadline. */
    uint64_t deadline=v.command_until_us;
    vl_packet(&v,0,&q,30000,&r); assert(vp_get_u16(r.payload+4)==VL_SEQUENCE && v.command_until_us==deadline);
    VpFrame renew={.type=VP_LEASE,.session=22,.seq=++q.seq,.payload_len=8};
    vp_put_u32(renew.payload,old_token); vp_put_u32(renew.payload+4,v.epoch);
    vl_packet(&v,0,&renew,30000,&r); assert(r.type==VP_LEASE && v.peer[0].token!=old_token);
    q.seq++; vl_packet(&v,0,&q,31000,&r); assert(vp_get_u16(r.payload+4)==VL_EXPIRED && v.command_until_us==deadline);
    /* Heartbeat cannot keep motion alive. Expiration revokes the epoch. */
    uint32_t epoch=v.epoch;
    q.type=VP_HEARTBEAT; q.payload_len=0; q.seq++; vl_packet(&v,0,&q,100000,&r);
    vl_tick(&v,104000,0,1,1); assert(v.state==VL_FAILSAFE && v.epoch!=epoch && !vl_permitted(&v,104000));
    /* Physical fault stays latched after inputs recover. */
    vl_tick(&v,105000,VL_FAULT_HARDWARE,0,1); vl_tick(&v,106000,0,1,1); assert(v.state==VL_FAULT);
    q.type=VP_ARM; q.payload_len=4; memset(q.payload,0,4); q.payload[0]=2; q.seq++;
    vl_packet(&v,0,&q,107000,&r); assert(v.state==VL_DISARMED && v.fault_bits==0);
    /* A new session revokes an outstanding arm grant. */
    q.payload[0]=1; q.payload[1]=VL_PI_ACTIVE; q.seq++; vl_packet(&v,0,&q,108000,&r); assert(v.arm_pending);
    q.type=VP_HELLO; q.payload_len=0; q.session=23; q.seq=1;
    vl_packet(&v,0,&q,109000,&r); assert(v.state==VL_DISARMED && !v.arm_pending && !v.peer[0].token);
}
static void sensor_test(void) {
    uint8_t raw[47]={0x54,0x2c}, result[47];
    vp_put_u16(raw+2,3600); vp_put_u16(raw+4,35900); vp_put_u16(raw+42,100); vp_put_u16(raw+44,29999);
    raw[46]=vs_lidar_crc(raw,46); assert(vs_lidar_crc((const uint8_t *)"\x01",1)==0x4d);
    VsLidar lidar={0};
    /* Noise and truncation must retain a nested next header. */
    for (unsigned i=0;i<10;i++) vs_lidar_feed(&lidar,raw[i],result);
    int frames=0; for (unsigned i=0;i<47;i++) frames+=vs_lidar_feed(&lidar,raw[i],result)==1;
    assert(frames==1 && !memcmp(raw,result,47));
    uint8_t data[92]={0}, wire[264], payload[64];
    vp_put_u16(data+4,2024); data[6]=1; data[7]=1; data[11]=7;
    vp_put_u32(data+16,(uint32_t)(int32_t)-500000000); data[20]=3; data[21]=1; data[23]=12;
    vp_put_u32(data+24,1210000000); vp_put_u32(data+28,310000000); vp_put_u32(data+32,12345);
    assert(vs_gnss_payload(data,92,1234567,payload)); assert(vp_get_u64(payload+8)==UINT64_C(1704067199500));
    assert(vp_get_u32(payload+16)==310000000 && vp_get_u32(payload+52)==3);
    data[78]=1; vs_gnss_payload(data,92,1234567,payload); assert(vp_get_u32(payload+52)==2); data[78]=0;
    size_t n=vs_ubx_encode(1,7,data,92,wire,sizeof(wire)); assert(n==100);
    VsUbx ubx={0}; frames=0;
    uint8_t corrupt[]={0xb5,0x62,0,0,120,0};
    for (unsigned i=0;i<sizeof(corrupt);i++) vs_ubx_feed(&ubx,corrupt[i]);
    for (unsigned i=0;i<10;i++) vs_ubx_feed(&ubx,0);
    for (unsigned i=0;i<n;i++) frames+=vs_ubx_feed(&ubx,wire[i])==1;
    for (unsigned i=0;i<30;i++) frames+=vs_ubx_feed(&ubx,0)==1;
    assert(frames==1 && !memcmp(ubx.frame,wire,n) && ubx.rejected);
    for (unsigned len=0;len<=VS_UBX_MAX;len++) {
        uint8_t source[VS_UBX_MAX];
        for (unsigned i=0;i<len;i++) source[i]=(uint8_t)(i*31+len);
        n=vs_ubx_encode(0x99,0x55,source,(uint16_t)len,wire,sizeof(wire));
        memset(&ubx,0,sizeof(ubx)); frames=0;
        for (unsigned i=0;i<n;i++) frames+=vs_ubx_feed(&ubx,wire[i])==1;
        assert(frames==1 && !memcmp(ubx.frame,wire,n));
    }
    n=vs_gnss_config(wire,sizeof(wire)); assert(n==47 && wire[2]==6 && wire[3]==0x8a && wire[7]==1);
    uint8_t imu[14]={0}, ip[48]; imu[2]=0x20; /* +1g X, big endian. */
    assert(vs_imu_payload(imu,99,3,0,ip)); assert(vp_get_u32(ip+8)==9806650 && vp_get_u32(ip+32)==25000);
    imu[0]=0x7f; imu[1]=0xff; assert(vs_imu_payload(imu,99,4,0,ip)); /* No signed multiply overflow. */
    imu[2]=0x80; imu[3]=0; assert(!vs_imu_payload(imu,99,5,0,ip));
}
static void tx_test(void) {
    VtPool pool={0}; VpFrame f={.type=VP_LIDAR,.payload_len=1}, out;
    for (unsigned i=0;i<20;i++) { f.payload[0]=(uint8_t)i; assert(vt_push(&pool,0,VT_SCAN,&f)); }
    assert(pool.dropped==12);
    f.type=VP_ODOM; f.payload[0]=1; vt_push(&pool,0,VT_PERIODIC,&f);
    f.payload[0]=2; vt_push(&pool,0,VT_PERIODIC,&f); assert(pool.coalesced==1);
    f.type=VP_ACK; vt_push(&pool,0,VT_CONTROL,&f);
    assert(vt_pop(&pool,0,&out) && out.type==VP_ACK);
    assert(vt_pop(&pool,0,&out) && out.type==VP_ODOM && out.payload[0]==2);
    for (unsigned i=12;i<20;i++) assert(vt_pop(&pool,0,&out) && out.type==VP_LIDAR && out.payload[0]==i);
    assert(!vt_pop(&pool,0,&out));
    for (unsigned i=0;i<VT_BLOCKS;i++) { f.type=(uint8_t)i; assert(vt_push(&pool,0,VT_PERIODIC,&f)); }
    assert(!vt_push(&pool,0,VT_SCAN,&f));
    assert(vt_push(&pool,1,VT_CONTROL,&f)); assert(vt_pop(&pool,1,&out));
}
static void lidar_power_test(void) {
    VsLidarPower power={0};
    assert(!vs_lidar_power_step(&power,0,0,1,0));
    assert(!vs_lidar_power_step(&power,0xffffffe0u,1,1,0));
    assert(!vs_lidar_power_step(&power,0x11,1,1,0));
    assert(vs_lidar_power_step(&power,0x12,1,1,0));
    assert(!vs_lidar_power_step(&power,0x13,1,0,0));
    assert(!vs_lidar_power_step(&power,0x14,1,1,1));
    assert(!vs_lidar_power_step(&power,1000,1,1,0));
    assert(!vs_lidar_power_step(&power,2000,1,1,0));
}
static void sbus_test(void) {
    VsSbus sbus={0}; uint8_t raw[25], payload[48];
    memset(raw,255,sizeof(raw)); raw[0]=15; raw[23]=3; raw[24]=0;
    assert(vs_sbus_payload(&sbus,0,payload)==0);
    for (unsigned i=0;i<25;i++) assert(vs_sbus_feed(&sbus,raw[i],1000)==(i==24 ? 1 : 0));
    assert(vs_sbus_payload(&sbus,100999,payload)==0x303);
    for (unsigned i=0;i<16;i++) assert(vp_get_u16(payload+8+2*i)==2047);
    assert(vs_sbus_payload(&sbus,101000,payload)==0x301);
    vs_sbus_lost(&sbus); assert(vs_sbus_payload(&sbus,1001,payload)==0x313);
    /* Long arbitrary input exercises bounded storage and resynchronization. */
    uint32_t rng=77;
    for (unsigned i=0;i<100000;i++) {
        rng=rng*1664525+1013904223;
        vs_sbus_feed(&sbus,(uint8_t)(rng>>24),i*120u);
        assert(sbus.used<25);
    }
    vs_sbus_lost(&sbus);
    for (unsigned i=0;i<25;i++) vs_sbus_feed(&sbus,raw[i],12000001);
    assert(vs_sbus_payload(&sbus,12000002,payload)==0x303);
}
static void pi_wifi_independence_test(void) {
    VlLink v; vl_init(&v,123);
    assert(!v.pi_ready);
    v.state=VL_WIFI_ACTIVE; v.source=VL_WIFI;
    v.command_until_us=100000; v.peer[VL_WIFI].lease_until_us=100000;
    vl_pi_inputs(&v,0,0,1);
    assert(v.state==VL_WIFI_ACTIVE && vl_permitted(&v,1000));
    vl_tick(&v,100000,0,1,1);
    assert(v.state==VL_FAILSAFE && !vl_permitted(&v,100000));
}
static void pi_power_test(void) {
    VwPower p={0};
    vw_step(&p,UINT32_MAX-50,1,1,0,1,1); assert(p.state==VW_WAIT_STOP && !p.request_out && !p.cut_out);
    vw_step(&p,49,0,1,0,1,1); assert(p.state==VW_WAIT_HALT && p.request_out && !p.cut_out);
    vw_step(&p,59,0,1,1,1,1); assert(!p.cut_out);
    vw_step(&p,79,0,1,1,1,1); assert(p.state==VW_CUT_PULSE && p.cut_out);
    vw_step(&p,98,0,0,1,1,1); assert(p.cut_out);
    vw_step(&p,99,0,0,1,1,1); assert(!p.cut_out && p.state==VW_WAIT_OFF);
    vw_step(&p,109,0,0,0,1,1); vw_step(&p,129,0,0,0,1,1);
    assert(p.state==VW_OFF && !p.request_out && p.inhibit);
    VlLink v; vl_init(&v,1); v.state=VL_WIFI_ACTIVE; v.source=VL_WIFI;
    v.command_until_us=100000; v.peer[VL_WIFI].lease_until_us=100000;
    vl_power_update(&v,1); assert(!vl_permitted(&v,1000) && v.state==VL_DISARMED);
    vl_power_update(&v,0); assert(v.state==VL_DISARMED && !vl_permitted(&v,1000));
}
int main(void) { protocol_test(); control_test(); link_test(); sensor_test(); tx_test(); sbus_test(); lidar_power_test(); pi_wifi_independence_test(); pi_power_test(); puts("protocol/control/session/sensor/TX/SBUS/Pi power tests passed"); return 0; }
