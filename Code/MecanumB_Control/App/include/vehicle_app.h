#ifndef VEHICLE_APP_H
#define VEHICLE_APP_H
#include "stm32f4xx_hal.h"

/* Runtime evidence. Stack values are free words (4 bytes each), not bytes. */
typedef struct {
    uint16_t stack_free[7]; /* Safety, Control, HostRx, Telemetry, Power, Imu, SensorRx */
    uint32_t jitter_last_us, jitter_max_us;
    uint8_t cpu_percent;
    uint32_t config_seq, config_errors;
} VehicleStats;

void vehicle_app_init(void);
void vehicle_app_stats(VehicleStats *out);
void vehicle_app_timer(TIM_HandleTypeDef *timer);
void vehicle_emergency_stop(void);
/* Task-context only; all payload bytes are copied before returning. */
uint64_t vehicle_time_us(void);
int vehicle_send_telemetry(uint8_t type, const uint8_t *data, uint16_t len, uint64_t now, unsigned priority);
#endif
