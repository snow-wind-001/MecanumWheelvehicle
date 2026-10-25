#include "vehicle_sensors.h"
#include "vehicle_protocol.h"
#include <string.h>

int vs_lidar_power_step(VsLidarPower *p, uint32_t now, int aux_good, int rx_ready, int branch_fault) {
    if (branch_fault) p->fault_latched=1;
    if (!aux_good || !rx_ready || p->fault_latched) { p->stable=0; p->enabled=0; return 0; }
    if (!p->stable) { p->stable=1; p->stable_since_ms=now; }
    p->enabled=(uint32_t)(now-p->stable_since_ms)>=50;
    return p->enabled;
}

void vs_sbus_lost(VsSbus *p) {
    p->used=0; p->transport_fault=1;
}
int vs_sbus_feed(VsSbus *p, uint8_t byte, uint64_t now) {
    if (p->used && (now<p->started_us || now-p->started_us>=10000)) {
        p->rejected++; vs_sbus_lost(p);
    }
    if (!p->used) {
        if (byte!=0x0f) return 0;
        p->started_us=now;
    }
    p->data[p->used++]=byte;
    if (p->used<25) return 0;
    uint8_t footer=p->data[24];
    int valid_footer=footer==0 || footer==0x04 || footer==0x14 || footer==0x24 || footer==0x34;
    if (!valid_footer || (p->data[23]&0xf0)) {
        p->rejected++; p->transport_fault=1;
        unsigned next=1;
        while (next<25 && p->data[next]!=0x0f) next++;
        p->used=(uint8_t)(25-next);
        memmove(p->data,p->data+next,p->used);
        /* A retained suffix has no independent timestamp; keep the older start. */
        return -1;
    }
    for (unsigned c=0;c<16;c++) {
        uint16_t value=0;
        for (unsigned b=0;b<11;b++) {
            unsigned bit=11*c+b;
            value|=(uint16_t)(((p->data[1+bit/8]>>(bit%8))&1u)<<b);
        }
        p->channels[c]=value;
    }
    p->flags=p->data[23]; p->sample_us=now; p->seen=1;
    p->transport_fault=0; p->accepted++; p->used=0;
    return 1;
}
uint32_t vs_sbus_payload(const VsSbus *p, uint64_t now, uint8_t out[48]) {
    uint32_t flags=p->seen ? VS_SBUS_SEEN : 0;
    if (p->seen && now>=p->sample_us && now-p->sample_us<100000) flags|=VS_SBUS_FRESH;
    flags|=(uint32_t)(p->flags&0x0c) | ((uint32_t)(p->flags&3)<<8);
    if (p->transport_fault) flags|=VS_SBUS_TRANSPORT;
    vp_put_u64(out,p->sample_us);
    for (unsigned i=0;i<16;i++) vp_put_u16(out+8+2*i,p->channels[i]);
    vp_put_u32(out+40,p->accepted); vp_put_u32(out+44,flags);
    return flags;
}

uint8_t vs_lidar_crc(const uint8_t *data, size_t len) {
    uint8_t crc=0;
    for (size_t i=0;i<len;i++) {
        crc^=data[i];
        for (unsigned b=0;b<8;b++) crc=(uint8_t)((crc&0x80) ? (crc<<1)^0x4d : crc<<1);
    }
    return crc;
}
int vs_lidar_valid(const uint8_t d[47]) {
    unsigned start=vp_get_u16(d+4), end=vp_get_u16(d+42);
    unsigned span=(end+36000-start)%36000;
    return d[0]==0x54 && d[1]==0x2c && vp_get_u16(d+2) && start<36000 && end<36000 &&
        span && span<18000 && vp_get_u16(d+44)<30000 && vs_lidar_crc(d,46)==d[46];
}
int vs_lidar_feed(VsLidar *p, uint8_t byte, uint8_t frame[47]) {
    if (!p->used && byte!=0x54) return 0;
    p->data[p->used++]=byte;
    if (p->used==2 && p->data[1]!=0x2c) {
        p->data[0]=byte; p->used=(byte==0x54); return 0;
    }
    if (p->used<47) return 0;
    if (vs_lidar_valid(p->data)) { memcpy(frame,p->data,47); p->used=0; p->accepted++; return 1; }
    p->rejected++;
    /* Keep any nested header so a truncated/corrupt packet cannot consume the
     * following valid packet's complete prefix. Work is bounded to 46 bytes. */
    unsigned next=1;
    while (next<47 && !(p->data[next]==0x54 && (next==46 || p->data[next+1]==0x2c))) next++;
    p->used=(uint8_t)(47-next); memmove(p->data,p->data+next,p->used); return -1;
}
size_t vs_ubx_encode(uint8_t cls, uint8_t id, const uint8_t *data, uint16_t len, uint8_t *wire, size_t capacity) {
    if (len>VS_UBX_MAX || capacity<(size_t)len+8) return 0;
    wire[0]=0xb5; wire[1]=0x62; wire[2]=cls; wire[3]=id; vp_put_u16(wire+4,len);
    if (len) memcpy(wire+6,data,len);
    uint8_t a=0,b=0;
    for (size_t i=2;i<(size_t)len+6;i++) { a+=wire[i]; b+=a; }
    wire[len+6]=a; wire[len+7]=b; return len+8u;
}
static void ubx_recover(VsUbx *p) {
    unsigned next=1;
    while (next<p->used && !(p->data[next]==0xb5 && (next+1==p->used || p->data[next+1]==0x62))) next++;
    p->used=(uint16_t)(p->used-next); memmove(p->data,p->data+next,p->used); p->expected=0;
}
int vs_ubx_feed(VsUbx *p, uint8_t byte) {
    if (!p->used && byte!=0xb5) return 0;
    if (p->used>=sizeof(p->data)) { p->used=0; p->expected=0; p->rejected++; return -1; }
    p->data[p->used++]=byte;
    if (p->used==2 && p->data[1]!=0x62) { p->data[0]=byte; p->used=(byte==0xb5); return 0; }
    int rejected=0;
    while (p->used>=6) {
        if (p->data[0]!=0xb5 || p->data[1]!=0x62) { ubx_recover(p); continue; }
        uint16_t len=vp_get_u16(p->data+4);
        if (len>VS_UBX_MAX) { p->rejected++; rejected=1; ubx_recover(p); continue; }
        p->expected=(uint16_t)(len+8);
        if (p->used<p->expected) break;
        uint8_t a=0,b=0;
        for (unsigned i=2;i<(unsigned)len+6;i++) { a+=p->data[i]; b+=a; }
        if (a!=p->data[len+6] || b!=p->data[len+7]) { p->rejected++; rejected=1; ubx_recover(p); continue; }
        memcpy(p->frame,p->data,p->expected);
        p->used=(uint16_t)(p->used-p->expected);
        memmove(p->data,p->data+p->expected,p->used);
        p->expected=0; p->accepted++; return 1;
    }
    return rejected ? -1 : 0;
}

static int leap(unsigned y) { return y%4==0 && (y%100!=0 || y%400==0); }
static uint64_t utc_ms(const uint8_t *d) {
    unsigned y=vp_get_u16(d+4), m=d[6], day=d[7], hour=d[8], min=d[9], sec=d[10];
    static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    /* Unix milliseconds cannot uniquely label a leap second; report invalid. */
    if ((d[11]&7)!=7 || y<2000 || y>2099 || !m || m>12 || !day ||
        day>days[m-1]+(m==2 && leap(y)) || hour>23 || min>59 || sec>59) return 0;
    int32_t nano=(int32_t)vp_get_u32(d+16); if (nano<-1000000000 || nano>1000000000) return 0;
    int64_t n=0;
    for (unsigned year=1970;year<y;year++) n+=365+leap(year);
    for (unsigned month=1;month<m;month++) n+=days[month-1]+(month==2 && leap(y));
    n=((n+day-1)*86400+hour*3600+min*60+sec)*1000;
    int64_t millis=nano>=0 ? nano/1000000 : -((-(int64_t)nano+999999)/1000000);
    return (uint64_t)(n+millis);
}
int vs_gnss_payload(const uint8_t *d, size_t len, uint64_t us, uint8_t p[64]) {
    if (len!=92) return 0;
    memset(p,0,64); vp_put_u64(p,us); vp_put_u64(p+8,utc_ms(d));
    memcpy(p+16,d+28,4); memcpy(p+20,d+24,4); memcpy(p+24,d+32,4);
    memcpy(p+28,d+48,12); memcpy(p+40,d+40,8); memcpy(p+48,d+68,4);
    int32_t lat=(int32_t)vp_get_u32(d+28), lon=(int32_t)vp_get_u32(d+24);
    uint32_t flags=0;
    if ((d[20]==3 || d[20]==4) && (d[21]&1) && !(vp_get_u16(d+78)&1) &&
        lat>=-900000000 && lat<=900000000 && lon>=-1800000000 && lon<=1800000000) flags|=1;
    if (vp_get_u64(p+8)) flags|=2;
    vp_put_u32(p+52,flags); p[56]=d[20]; p[57]=d[23]; memcpy(p+60,d,4); return 1;
}
size_t vs_gnss_config(uint8_t *wire, size_t capacity) {
    uint8_t p[48]={0,1,0,0}; size_t n=4; /* VALSET version0, RAM only. */
    const uint32_t keys[]={0x40520001,0x10740001,0x10740002,0x20910007,0x30210001,0x30210002};
    const uint32_t values[]={115200,1,0,1,500,1}; /* 2Hz fits default multi-GNSS limits. */
    const unsigned widths[]={4,1,1,1,2,2};
    for (unsigned i=0;i<6;i++) {
        vp_put_u32(p+n,keys[i]); n+=4;
        for (unsigned b=0;b<widths[i];b++) p[n++]=(uint8_t)(values[i]>>(8*b));
    }
    return vs_ubx_encode(6,0x8a,p,(uint16_t)n,wire,capacity);
}
static int32_t be_i16(const uint8_t *p) { uint32_t v=(uint32_t)p[0]<<8|p[1]; return v&0x8000 ? (int32_t)v-65536 : (int32_t)v; }
int vs_imu_payload(const uint8_t d[14], uint64_t us, uint32_t seq, uint32_t flags, uint8_t p[48]) {
    memset(p,0,48); vp_put_u64(p,us);
    int32_t temperature=be_i16(d);
    for (unsigned i=0;i<3;i++) {
        int32_t accel=be_i16(d+2+2*i), gyro=be_i16(d+8+2*i);
        if (accel<=-32767 || gyro<=-32767 || temperature<=-32767) return 0;
        /* ±4g: 8192 LSB/g; ±500deg/s: 65.5 LSB/(deg/s). */
        vp_put_u32(p+8+4*i,(uint32_t)(int32_t)((int64_t)accel*9806650/8192));
        vp_put_u32(p+20+4*i,(uint32_t)(int32_t)((int64_t)gyro*17453293/65500));
    }
    vp_put_u32(p+32,(uint32_t)(int32_t)(25000+(int64_t)temperature*100000/13248));
    vp_put_u32(p+36,seq); vp_put_u32(p+40,flags|1u); return 1;
}
