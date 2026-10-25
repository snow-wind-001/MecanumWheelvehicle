#include "vehicle_sensor_app.h"
#include "vehicle_sensors.h"
#include "vehicle_protocol.h"
#include "vehicle_tx.h"
#include "vehicle_health.h"
#include "vehicle_app.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

extern UART_HandleTypeDef huart2, huart3, huart7;
extern SPI_HandleTypeDef hspi2;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t *dma;
    uint16_t capacity, position;
    volatile uint32_t produced, errors;
    uint32_t consumed, seen_errors, last_poll_ms;
} SensorPort;
static uint8_t gnss_dma[1024], lidar_dma[4096], sbus_dma[128];
static SensorPort ports[3];
static VsUbx ubx;
static VsLidar lidar;
static VsSbus sbus;
static VsLidarPower lidar_power;
static uint8_t lidar_sample_valid;
static uint8_t lidar_batch[221], gnss_config_wire[64];
static uint8_t lidar_count, gnss_phase, gnss_candidate;
static uint32_t gnss_deadline_ms, gnss_last_ms, lidar_last_ms, imu_last_ms;
static uint64_t lidar_batch_started;
static volatile uint8_t gnss_tx_busy;
static uint32_t imu_frames, imu_errors, gnss_frames, gnss_errors, lidar_frames, uart_losses;
static volatile uint32_t rx_progress, imu_progress, adc_frames, adc_last_ms, adc_errors;
static volatile uint32_t imu_edge_seq, imu_edge_cycle;
static uint16_t adc_dma[7*32] __attribute__((aligned(4)));
static volatile uint16_t adc_latest[7];
static StaticTask_t rx_tcb, imu_tcb;
static TaskHandle_t rx_task, imu_task;
static StackType_t rx_stack[1024], imu_stack[768];

static void harvest(SensorPort *p) {
    if (!p->uart || p->uart->RxState!=HAL_UART_STATE_BUSY_RX || !p->uart->hdmarx) return;
    uint16_t pos=(uint16_t)(p->capacity-__HAL_DMA_GET_COUNTER(p->uart->hdmarx));
    if (pos==p->capacity) pos=0;
    p->produced+=pos>=p->position ? (uint32_t)(pos-p->position) : (uint32_t)p->capacity-p->position+pos;
    p->position=pos;
}
void vehicle_sensors_uart_rx(UART_HandleTypeDef *uart) {
    for (unsigned i=0;i<3;i++) if (ports[i].uart==uart) harvest(&ports[i]);
}
void vehicle_sensors_uart_error(UART_HandleTypeDef *uart) {
    for (unsigned i=0;i<3;i++) if (ports[i].uart==uart) ports[i].errors++;
}
void vehicle_sensors_uart_tx(UART_HandleTypeDef *uart) { if (uart==&huart2) gnss_tx_busy=0; }
void vehicle_sensors_exti(uint16_t pin) {
    if (pin==IMU_DRDY_Pin) { imu_edge_cycle=DWT->CYCCNT; imu_edge_seq++; }
}
void vehicle_sensors_progress(uint32_t *rx, uint32_t *imu) { *rx=rx_progress; *imu=imu_progress; }

static int start_rx(SensorPort *p) {
    HAL_UART_AbortReceive(p->uart);
    taskENTER_CRITICAL();
    p->position=0; p->produced=0; p->consumed=0; p->last_poll_ms=HAL_GetTick();
    taskEXIT_CRITICAL();
    return HAL_UARTEx_ReceiveToIdle_DMA(p->uart,p->dma,p->capacity)==HAL_OK;
}
static int gnss_baud(uint32_t baud) {
    HAL_UART_AbortTransmit(&huart2); gnss_tx_busy=0;
    HAL_UART_AbortReceive(&huart2);
    huart2.Init.BaudRate=baud;
    memset(&ubx,0,sizeof(ubx));
    return HAL_UART_Init(&huart2)==HAL_OK && start_rx(&ports[0]);
}
static void gnss_step(uint32_t ms) {
    static const uint32_t bauds[]={115200,9600,38400,57600};
    if (gnss_phase==0 && (int32_t)(ms-gnss_deadline_ms)>=0) {
        if (!gnss_baud(bauds[gnss_candidate])) { gnss_errors++; gnss_deadline_ms=ms+1000; return; }
        size_t n=vs_gnss_config(gnss_config_wire,sizeof(gnss_config_wire));
        gnss_tx_busy=1;
        if (HAL_UART_Transmit_IT(&huart2,gnss_config_wire,(uint16_t)n)!=HAL_OK) {
            gnss_tx_busy=0; gnss_errors++; gnss_deadline_ms=ms+1000; return;
        }
        gnss_phase=1; gnss_deadline_ms=ms+200;
    } else if (gnss_phase==1) {
        if (!gnss_tx_busy) {
            if (!gnss_baud(115200)) gnss_errors++;
            gnss_phase=2; gnss_deadline_ms=ms+1500;
        } else if ((int32_t)(ms-gnss_deadline_ms)>=0) {
            HAL_UART_AbortTransmit(&huart2); gnss_tx_busy=0; gnss_errors++;
            gnss_candidate=(gnss_candidate+1)%4; gnss_phase=0; gnss_deadline_ms=ms+100;
        }
    } else if ((gnss_phase==2 && (int32_t)(ms-gnss_deadline_ms)>=0) ||
               (gnss_phase==3 && (uint32_t)(ms-gnss_last_ms)>2500)) {
        gnss_errors++; gnss_candidate=(gnss_candidate+1)%4;
        gnss_phase=0; gnss_deadline_ms=ms+100;
    }
}
static void lidar_flush(uint64_t now) {
    if (!lidar_count) return;
    lidar_batch[0]=lidar_count;
    vehicle_send_telemetry(VP_LIDAR,lidar_batch,(uint16_t)(1+55*lidar_count),now,VT_SCAN);
    lidar_count=0;
}
static void received_byte(unsigned index, uint8_t byte, uint64_t now) {
    if (index==2) {
        vs_sbus_feed(&sbus,byte,now);
    } else if (index==0) {
        uint32_t rejected=ubx.rejected;
        if (vs_ubx_feed(&ubx,byte)==1) {
            const uint8_t *f=ubx.frame; unsigned len=vp_get_u16(f+4);
            if (f[2]==1 && f[3]==7 && len==92) {
                uint8_t p[64];
                if (vs_gnss_payload(f+6,len,now,p)) {
                    gnss_frames++; gnss_last_ms=HAL_GetTick();
                    if (gnss_phase==2) gnss_phase=3;
                    vehicle_send_telemetry(VP_GNSS,p,sizeof(p),now,VT_PERIODIC);
                }
            } else if (f[2]==5 && f[3]==0 && len==2 && f[6]==6 && f[7]==0x8a) gnss_errors++;
        }
        gnss_errors+=ubx.rejected-rejected;
    } else {
        if (!lidar_power.enabled) return;
        uint8_t raw[47];
        if (vs_lidar_feed(&lidar,byte,raw)==1) {
            if (!lidar_count) lidar_batch_started=now;
            unsigned offset=1+55*lidar_count;
            vp_put_u64(lidar_batch+offset,now); memcpy(lidar_batch+offset+8,raw,47);
            lidar_count++; lidar_frames++; lidar_last_ms=HAL_GetTick(); lidar_sample_valid=1;
            if (lidar_count==4) lidar_flush(now);
        }
    }
}
static void drain(unsigned index, uint64_t now) {
    SensorPort *p=&ports[index];
    uint32_t ms=HAL_GetTick(), gap=ms-p->last_poll_ms;
    p->last_poll_ms=ms;
    taskENTER_CRITICAL(); harvest(p); uint32_t produced=p->produced; taskEXIT_CRITICAL();
    uint32_t max_gap=index==0 ? 40u : index==1 ? 80u : 10u;
    if (gap>=max_gap || p->errors!=p->seen_errors ||
        produced-p->consumed>p->capacity || p->uart->RxState!=HAL_UART_STATE_BUSY_RX) {
        uart_losses++; p->seen_errors=p->errors;
        if (!index) memset(&ubx,0,sizeof(ubx));
        else if (index==1) { lidar.used=0; lidar_count=0; }
        else vs_sbus_lost(&sbus);
        if (!start_rx(p)) p->errors++;
        return;
    }
    unsigned budget=512;
    while (p->consumed!=produced && budget--) {
        uint8_t byte=p->dma[p->consumed++%p->capacity];
        received_byte(index,byte,now);
    }
}

static void adc_copy(unsigned last_scan) {
    for (unsigned i=0;i<7;i++) adc_latest[i]=adc_dma[last_scan+i];
    adc_frames++; adc_last_ms=HAL_GetTick();
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc) { if (adc==&hadc1) adc_copy(7*15); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc) { if (adc==&hadc1) adc_copy(7*31); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc) { if (adc==&hadc1) adc_errors++; }
int vehicle_sensors_voltages(uint16_t *vbat, uint16_t *vm) {
    uint16_t raw[7]; int valid=vehicle_sensors_adc(raw);
    if (valid) {
        *vbat=vh_pack_mv(raw[4],VH_VBAT_DIVIDER);
        *vm=vh_pack_mv(raw[5],VH_VM_DIVIDER);
    }
    return valid;
}
int vehicle_sensors_adc(uint16_t raw[7]) {
    taskENTER_CRITICAL();
    int valid=adc_frames && (uint32_t)(HAL_GetTick()-adc_last_ms)<100;
    if (valid) for (unsigned i=0;i<7;i++) raw[i]=adc_latest[i];
    taskEXIT_CRITICAL();
    return valid;
}
static void sensor_rx_run(void *arg) {
    (void)arg;
    /* Default internal 10 Hz control: never drive the optional PWM while off.
     * TIM9 is configured for 30 kHz but is not started in this mode. */
    HAL_GPIO_WritePin(GPIOE,GPIO_PIN_5,GPIO_PIN_RESET);
    GPIO_InitTypeDef pwm={.Pin=GPIO_PIN_5,.Mode=GPIO_MODE_OUTPUT_PP,.Pull=GPIO_PULLDOWN,.Speed=GPIO_SPEED_FREQ_LOW};
    HAL_GPIO_Init(GPIOE,&pwm);
    HAL_GPIO_WritePin(LD06_PWR_EN_GPIO_Port,LD06_PWR_EN_Pin,GPIO_PIN_RESET);
    if (!start_rx(&ports[1])) ports[1].errors++;
    if (!start_rx(&ports[2])) ports[2].errors++;
    if (HAL_ADC_Start_DMA(&hadc1,(uint32_t *)adc_dma,sizeof(adc_dma)/sizeof(adc_dma[0]))!=HAL_OK || HAL_TIM_Base_Start(&htim8)!=HAL_OK) adc_errors++;
    uint32_t adc_retry_ms=HAL_GetTick()+1000;
    uint32_t sbus_report_ms=HAL_GetTick();
    for (;;) {
        uint32_t ms=HAL_GetTick(); uint64_t now=vehicle_time_us();
        if ((int32_t)(ms-adc_retry_ms)>=0) {
            if (!adc_frames || (uint32_t)(ms-adc_last_ms)>=100) {
                HAL_TIM_Base_Stop(&htim8); HAL_ADC_Stop_DMA(&hadc1);
                if (HAL_ADC_Start_DMA(&hadc1,(uint32_t *)adc_dma,sizeof(adc_dma)/sizeof(adc_dma[0]))!=HAL_OK || HAL_TIM_Base_Start(&htim8)!=HAL_OK) adc_errors++;
            }
            adc_retry_ms=ms+1000;
        }
        gnss_step(ms);
        int old_enable=lidar_power.enabled;
        int enable=vs_lidar_power_step(&lidar_power,ms,
            HAL_GPIO_ReadPin(AUX_PWR_GOOD_GPIO_Port,AUX_PWR_GOOD_Pin)==GPIO_PIN_SET,
            huart3.RxState==HAL_UART_STATE_BUSY_RX,
            HAL_GPIO_ReadPin(LD06_FAULT_N_GPIO_Port,LD06_FAULT_N_Pin)==GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LD06_PWR_EN_GPIO_Port,LD06_PWR_EN_Pin,enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
        if (enable!=old_enable) {
            lidar.used=0; lidar_count=0; lidar_sample_valid=0;
            taskENTER_CRITICAL(); harvest(&ports[1]); ports[1].consumed=ports[1].produced; taskEXIT_CRITICAL();
        }
        /* Drain the small remote-control ring before the longer sensor streams. */
        drain(2,now);
        for (unsigned i=0;i<2;i++) drain(i,now);
        if ((uint32_t)(ms-sbus_report_ms)>=50) {
            uint8_t payload[48]; vs_sbus_payload(&sbus,now,payload);
            vehicle_send_telemetry(VP_SBUS,payload,sizeof(payload),now,VT_PERIODIC);
            sbus_report_ms=ms;
        }
        if (lidar_count && now-lidar_batch_started>=10000) lidar_flush(now);
        rx_progress++; vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static int imu_transfer(uint8_t reg, uint8_t *data, uint16_t len, int write) {
    uint8_t tx[16]={0}, rx[16]={0};
    if (len>14) return 0;
    tx[0]=write ? reg : (reg|0x80);
    if (write) memcpy(tx+1,data,len);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port,IMU_CS_Pin,GPIO_PIN_RESET);
    HAL_StatusTypeDef result=HAL_SPI_TransmitReceive(&hspi2,tx,rx,(uint16_t)(len+1),2);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port,IMU_CS_Pin,GPIO_PIN_SET);
    if (result==HAL_OK && !write) memcpy(data,rx+1,len);
    return result==HAL_OK;
}
static int imu_write(uint8_t reg, uint8_t value) { return imu_transfer(reg,&value,1,1); }
static int imu_read(uint8_t reg, uint8_t *value) { return imu_transfer(reg,value,1,0); }
static void imu_run(void *arg) {
    (void)arg;
    unsigned phase=0; uint32_t deadline=0, last_data=0, sequence=0, last_edge=0;
    for (;;) {
        uint32_t ms=HAL_GetTick(); int failed=0;
        if (phase==0 && (int32_t)(ms-deadline)>=0) {
            uint8_t id=0;
            if (!imu_write(0x76,0) || !imu_read(0x75,&id) || id!=0x47 || !imu_write(0x11,1)) failed=1;
            else { phase=1; deadline=ms+2; }
        } else if (phase==1 && (int32_t)(ms-deadline)>=0) {
            uint8_t status=0;
            if (!imu_read(0x2d,&status) || !(status&0x10) || !imu_write(0x4c,0x33) ||
                !imu_write(0x4f,0x47) || !imu_write(0x50,0x47) || !imu_write(0x14,0x03) || !imu_write(0x64,0x00) ||
                !imu_write(0x65,0x08) || !imu_write(0x4e,0x0f)) failed=1;
            else { phase=2; deadline=HAL_GetTick()+50; }
        } else if (phase==2 && (int32_t)(ms-deadline)>=0) {
            uint8_t gyro=0,accel=0,power=0,endian=0;
            if (!imu_read(0x4f,&gyro) || !imu_read(0x50,&accel) || !imu_read(0x4e,&power) || !imu_read(0x4c,&endian) ||
                gyro!=0x47 || accel!=0x47 || power!=0x0f || endian!=0x33) failed=1;
            else { phase=3; last_data=ms; }
        } else if (phase==3) {
            uint8_t status=0;
            if (!imu_read(0x2d,&status)) failed=1;
            else if (status&8) {
                uint32_t edge, cycle;
                taskENTER_CRITICAL(); edge=imu_edge_seq; cycle=imu_edge_cycle; taskEXIT_CRITICAL();
                uint8_t data[14], payload[48];
                if (!imu_transfer(0x1d,data,sizeof(data),0)) failed=1;
                else {
                    uint64_t now=vehicle_time_us(), stamp=now; uint32_t flags=0;
                    taskENTER_CRITICAL(); uint32_t after=imu_edge_seq; taskEXIT_CRITICAL();
                    uint32_t delay=(DWT->CYCCNT-cycle)/(SystemCoreClock/1000000u);
                    if (edge && edge==after && edge!=last_edge && delay<10000 && now>=delay) { stamp=now-delay; flags|=2; }
                    if (last_edge && edge-last_edge>1) flags|=4;
                    last_edge=edge;
                    if (vs_imu_payload(data,stamp,++sequence,flags,payload)) {
                        imu_frames++; imu_last_ms=ms; last_data=ms;
                        vehicle_send_telemetry(VP_IMU,payload,sizeof(payload),now,VT_PERIODIC);
                    } else imu_errors++;
                }
            }
            if ((uint32_t)(ms-last_data)>100) failed=1;
        }
        if (failed) { imu_errors++; phase=0; deadline=HAL_GetTick()+1000; }
        imu_progress++; vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void vehicle_sensors_diagnostics(uint32_t v[12]) {
    taskENTER_CRITICAL(); uint32_t ms=HAL_GetTick();
    v[0]=(imu_frames && ms-imu_last_ms<100 ? 1u : 0u) | (gnss_phase==3 && ms-gnss_last_ms<2500 ? 2u : 0u) |
        (lidar_sample_valid && lidar_power.enabled && ms-lidar_last_ms<200 ? 4u : 0u) | (adc_frames && ms-adc_last_ms<100 ? 8u : 0u) |
        (lidar_power.enabled ? 16u : 0u) | (HAL_GPIO_ReadPin(AUX_PWR_GOOD_GPIO_Port,AUX_PWR_GOOD_Pin)==GPIO_PIN_SET ? 32u : 0u) |
        (lidar_power.fault_latched ? 64u : 0u);
    v[1]=imu_frames; v[2]=imu_errors; v[3]=gnss_frames; v[4]=gnss_errors; v[5]=lidar_frames;
    v[6]=lidar.rejected; v[7]=uart_losses; v[8]=adc_frames; v[9]=gnss_phase; v[10]=adc_errors; v[11]=0;
    taskEXIT_CRITICAL();
}
void vehicle_sensors_stack(uint16_t free_words[2]) {
    if (!free_words) return;
    free_words[0]=imu_task ? (uint16_t)uxTaskGetStackHighWaterMark(imu_task) : 0;
    free_words[1]=rx_task ? (uint16_t)uxTaskGetStackHighWaterMark(rx_task) : 0;
}
void vehicle_sensors_init(void) {
    ports[0]=(SensorPort){.uart=&huart2,.dma=gnss_dma,.capacity=sizeof(gnss_dma)};
    ports[1]=(SensorPort){.uart=&huart3,.dma=lidar_dma,.capacity=sizeof(lidar_dma)};
    ports[2]=(SensorPort){.uart=&huart7,.dma=sbus_dma,.capacity=sizeof(sbus_dma)};
    imu_task=xTaskCreateStatic(imu_run,"Imu",768,NULL,10,imu_stack,&imu_tcb); configASSERT(imu_task);
    rx_task=xTaskCreateStatic(sensor_rx_run,"SensorRx",1024,NULL,8,rx_stack,&rx_tcb); configASSERT(rx_task);
}
