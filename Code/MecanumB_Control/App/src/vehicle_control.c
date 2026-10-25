#include "vehicle_control.h"
#include <math.h>
#include <string.h>

static float clamp(float x, float lo, float hi) { return fminf(hi,fmaxf(lo,x)); }
int vc_config_valid(const VcConfig *c) {
    if (!c) return 0;
    const float values[]={c->radius_m,c->half_length_m,c->half_width_m,c->max_wheel_rad_s,
        c->max_wheel_accel_rad_s2,c->kp,c->ki,c->feedforward,c->duty_limit,c->speed_filter_s,c->reversal_rad_s};
    for (unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) if (!isfinite(values[i])) return 0;
    if (c->radius_m<.01f || c->radius_m>.5f || c->half_length_m<.01f || c->half_length_m>2 ||
        c->half_width_m<.01f || c->half_width_m>2 || !c->counts_per_rev || c->counts_per_rev>10000000 ||
        c->max_wheel_rad_s<=0 || c->max_wheel_rad_s>200 || c->max_wheel_accel_rad_s2<=0 ||
        c->max_wheel_accel_rad_s2>1000 || c->kp<0 || c->kp>10 || c->ki<0 || c->ki>100 ||
        c->feedforward<0 || c->feedforward>10 || c->duty_limit<=0 || c->duty_limit>1 ||
        c->speed_filter_s<.001f || c->speed_filter_s>1 || c->reversal_rad_s<0 || c->reversal_rad_s>10) return 0;
    /* TIM3/4 modular differences must stay within half their 16-bit range,
     * including the largest accepted 2ms sampling interval. */
    if ((double)c->counts_per_rev*c->max_wheel_rad_s*.002/6.28318530718>=32768) return 0;
    for (unsigned i=0;i<4;i++) if ((c->encoder_sign[i]!=1 && c->encoder_sign[i]!=-1) ||
        (c->motor_sign[i]!=1 && c->motor_sign[i]!=-1)) return 0;
    return 1;
}

void vc_init(VcControl *v, const VcConfig *c) {
    memset(v,0,sizeof(*v));
    if (vc_config_valid(c)) { v->config=*c; v->configured=1; }
    for (unsigned i=0;i<4;i++) v->direction[i]=1;
}
void vc_inverse(const VcConfig *c, float vx, float vy, float wz, float w[4]) {
    float turn=(c->half_length_m+c->half_width_m)*wz;
    w[0]=(vx-vy-turn)/c->radius_m; w[1]=(vx+vy+turn)/c->radius_m;
    w[2]=(vx+vy-turn)/c->radius_m; w[3]=(vx-vy+turn)/c->radius_m;
    float largest=0;
    for (unsigned i=0;i<4;i++) largest=fmaxf(largest,fabsf(w[i]));
    if (largest>c->max_wheel_rad_s) for (unsigned i=0;i<4;i++) w[i]*=c->max_wheel_rad_s/largest;
}
void vc_forward(const VcConfig *c, const float w[4], float body[3]) {
    float scale=c->radius_m*.25f;
    body[0]=scale*(w[0]+w[1]+w[2]+w[3]);
    body[1]=scale*(-w[0]+w[1]+w[2]-w[3]);
    body[2]=scale*(-w[0]+w[1]-w[2]+w[3])/(c->half_length_m+c->half_width_m);
}
int64_t vc_encoder_delta(uint32_t now, uint32_t old, unsigned bits) {
    if (bits!=16 && bits!=32) return 0;
    uint64_t modulus=UINT64_C(1)<<bits;
    uint64_t d=((uint64_t)now-old)&(modulus-1);
    return d>=modulus/2 ? (int64_t)d-(int64_t)modulus : (int64_t)d;
}

void vc_step(VcControl *v, const uint32_t counts[4], const float body[3], float dt, int permitted) {
    static const unsigned bits[4]={32,16,16,32};
    int timing_ok=isfinite(dt) && dt>=.0005f && dt<=.002f;
    if (!v->initialized) { memcpy(v->last_count,counts,sizeof(v->last_count)); v->initialized=1; }
    for (unsigned i=0;i<4;i++) {
        int64_t delta=vc_encoder_delta(counts[i],v->last_count[i],bits[i]);
        v->last_count[i]=counts[i];
        if (v->configured) {
            delta*=v->config.encoder_sign[i]; v->total[i]+=delta;
            if (timing_ok) {
                float speed=(float)delta*(6.28318530718f/v->config.counts_per_rev)/dt;
                v->measured[i]+=dt/(v->config.speed_filter_s+dt)*(speed-v->measured[i]);
            }
        }
    }
    if (!permitted || !v->configured || !timing_ok || !isfinite(body[0]) || !isfinite(body[1]) || !isfinite(body[2])) {
        memset(v->duty,0,sizeof(v->duty)); memset(v->target,0,sizeof(v->target));
        memset(v->integral,0,sizeof(v->integral)); return;
    }
    float desired[4], largest=0;
    vc_inverse(&v->config,body[0],body[1],body[2],desired);
    for (unsigned i=0;i<4;i++) largest=fmaxf(largest,fabsf(desired[i]-v->target[i]));
    float ratio=largest>0 ? fminf(1,v->config.max_wheel_accel_rad_s2*dt/largest) : 1;
    for (unsigned i=0;i<4;i++) {
        v->target[i]+=(desired[i]-v->target[i])*ratio;
        float target=v->target[i];
        int8_t direction=target<0 ? -1 : 1;
        if (fabsf(target)<.001f || (direction!=v->direction[i] && fabsf(v->measured[i])>v->config.reversal_rad_s)) {
            v->duty[i]=0; v->integral[i]=0; continue;
        }
        if (direction!=v->direction[i]) {
            v->direction[i]=direction; v->duty[i]=0; v->integral[i]=0; continue;
        }
        /* Magnitude control prevents a PI braking term reversing the bridge. */
        float error=direction*(target-v->measured[i]);
        float next=clamp(v->integral[i]+error*v->config.ki*dt,-v->config.duty_limit,v->config.duty_limit);
        float output=fabsf(target)*v->config.feedforward+v->config.kp*error+next;
        if ((output>=0 && output<=v->config.duty_limit) || (output>v->config.duty_limit && error<0) || (output<0 && error>0)) v->integral[i]=next;
        v->duty[i]=direction*clamp(fabsf(target)*v->config.feedforward+v->config.kp*error+v->integral[i],0,v->config.duty_limit);
    }
}
