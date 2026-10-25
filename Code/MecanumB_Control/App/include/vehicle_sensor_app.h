#ifndef VEHICLE_SENSOR_APP_H
#define VEHICLE_SENSOR_APP_H
#include "stm32f4xx_hal.h"
void vehicle_sensors_init(void);
void vehicle_sensors_uart_rx(UART_HandleTypeDef *uart);
void vehicle_sensors_uart_error(UART_HandleTypeDef *uart);
void vehicle_sensors_uart_tx(UART_HandleTypeDef *uart);
void vehicle_sensors_exti(uint16_t pin);
void vehicle_sensors_progress(uint32_t *rx, uint32_t *imu);
void vehicle_sensors_diagnostics(uint32_t values[12]);
int vehicle_sensors_voltages(uint16_t *vbat_mv, uint16_t *vm_mv);
/* Raw order: FL, FR, RL, RR current; VBAT; VM; NTC. Returns freshness. */
int vehicle_sensors_adc(uint16_t raw[7]);
/* Free stack words for the Imu and SensorRx tasks. */
void vehicle_sensors_stack(uint16_t free_words[2]);
#endif
