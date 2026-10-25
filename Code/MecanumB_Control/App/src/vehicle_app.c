#include "vehicle_app.h"
#include "vehicle_link.h"
#include "vehicle_mavlink.h"
#include "vehicle_tx.h"
#include "vehicle_sensor_app.h"
#include "vehicle_power.h"
#include "vehicle_store.h"
#include "vehicle_health.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>

/* 1: standard MAVLink v2 on both companion links (primary).
 * 0: keep the legacy "VP v1" framing (host tests / bring-up fallback). */
#ifndef VEHICLE_LINK_PROTOCOL_MAVLINK
#define VEHICLE_LINK_PROTOCOL_MAVLINK 1
#endif
static const int mavlink_mode = VEHICLE_LINK_PROTOCOL_MAVLINK;

extern TIM_HandleTypeDef htim1, htim2, htim3, htim4, htim5, htim6;
extern UART_HandleTypeDef huart1, huart6;
extern IWDG_HandleTypeDef hiwdg;

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t dma[2048];
    volatile uint32_t produced, errors;
    uint32_t consumed, last_poll_ms, seen_errors;
    volatile uint16_t position;
    VpParser parser;
    uint8_t tx_wire[VP_WIRE_MAX];
    volatile uint8_t tx_busy;
    uint32_t tx_started;
} HostPort;
static HostPort hosts[2];
static VtPool tx_pool;
static VlLink link;
static VcControl control;
static VmContext mavlink;
static TaskHandle_t control_task, safety_task, host_task, telemetry_task, power_task;
static StaticTask_t safety_tcb, control_tcb, host_tcb, telemetry_tcb, power_tcb;
static StackType_t safety_stack[512], control_stack[1024], host_stack[1536], telemetry_stack[1024];
static StackType_t power_stack[512];
static VwPower pi_power;
static uint16_t power_inputs;
static volatile uint32_t power_progress;
static volatile uint32_t urgent_fault, control_progress, host_progress, tx_progress;
static uint32_t control_overruns;
static uint32_t applied_revision;
static uint32_t mono_last_cycles;
static uint64_t mono_cycles;
static VsStore store;
static VhState health;
static volatile uint8_t store_busy, store_programming;
static volatile uint32_t store_release_ms;
static volatile uint32_t jitter_last_us, jitter_max_us;
static uint8_t cpu_percent;
static uint32_t stats_idle_last, stats_cycles_last;
static uint8_t stats_have;

/* Extend DWT before scaling: the 180 MHz raw counter wraps every 23.8 s.
 * Kernel statistics use microseconds (32-bit wrap after about 71 minutes).
 * Sampled at every context switch; execution must not stop for a full DWT wrap. */
uint32_t vehicle_runtime_counter(void) {
    static uint32_t previous;
    static uint64_t accumulated;
    uint32_t mask=__get_PRIMASK();
    __disable_irq();
    uint32_t current=DWT->CYCCNT;
    accumulated+=(uint32_t)(current-previous); previous=current;
    uint64_t snapshot=accumulated;
    __set_PRIMASK(mask);
    return (uint32_t)(snapshot/(SystemCoreClock/1000000u));
}

/* Boot only: keep TPS3431 WDI alive while single-bank Flash is busy.
 * Everything in this routine, including the literal pool, executes from SRAM;
 * interrupts are masked so no Flash-resident ISR can stall the feed loop.
 * No HAL/library call is allowed here. DWT is enabled before store_boot(). */
__attribute__((section(".RamFunc"), noinline))
static int flash_erase_ram(uint32_t sector, uint32_t clock_hz) {
    uint32_t primask=__get_PRIMASK();
    __disable_irq();
    GPIOG->BSRR=(uint32_t)MOTOR_ARM_REQ_Pin<<16;
    TIM1->BDTR&=~TIM_BDTR_MOE;
    uint32_t started=DWT->CYCCNT, fed=started;
    uint32_t interval=clock_hz/50u; /* Falling WDI edges every 20 ms; min tWD 170 ms. */
    uint32_t timeout=clock_hz*3u;   /* Bounded boot maintenance, 3 s at 180 MHz. */
    GPIOG->BSRR=MCU_WDI_Pin;
    FLASH->SR=FLASH_SR_EOP|FLASH_SR_OPERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR;
    FLASH->CR=(FLASH->CR & ~(FLASH_CR_PSIZE|FLASH_CR_SNB)) | FLASH_PSIZE_WORD | FLASH_CR_SER | (sector<<3);
    FLASH->CR|=FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY) {
        uint32_t elapsed=DWT->CYCCNT-started;
        if (elapsed>=timeout) {
            /* Do not return into busy Flash or feed a hung controller forever.
             * Stop servicing the external watchdog and let it reset the MCU. */
            for (;;) { __NOP(); }
        }
        if ((uint32_t)(DWT->CYCCNT-fed)>=interval) {
            GPIOG->BSRR=(uint32_t)MCU_WDI_Pin<<16;
            __DSB();
            for (volatile unsigned pulse=0;pulse<256u;pulse++) { __NOP(); }
            GPIOG->BSRR=MCU_WDI_Pin;
            fed=DWT->CYCCNT;
        }
    }
    uint32_t errors=FLASH->SR & (FLASH_SR_OPERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGPERR|FLASH_SR_PGSERR);
    FLASH->CR&=~(FLASH_CR_SER|FLASH_CR_SNB);
    __set_PRIMASK(primask);
    return errors==0;
}
static int flash_erase(uint32_t address, uint32_t size, void *ctx) {
    (void)ctx;
    if (size!=0x20000u || (address!=0x080C0000u && address!=0x080E0000u)) return 0;
    if (HAL_FLASH_Unlock()!=HAL_OK) return 0;
    int ok=flash_erase_ram(address==0x080C0000u ? 10u : 11u,SystemCoreClock);
    FLASH_FlushCaches(); /* Erase follows the boot scan; discard cached old slot data. */
    HAL_FLASH_Lock();
    return ok;
}
static int flash_program(uint32_t address, const void *data, uint32_t len, void *ctx) {
    (void)ctx;
    const uint32_t *words=data;
    if ((len&3u) || ((uintptr_t)data&3u)) return 0;
    HAL_FLASH_Unlock();
    int ok=1;
    for (uint32_t i=0;i<len;i+=4) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address+i,words[i/4])!=HAL_OK) { ok=0; break; }
    }
    HAL_FLASH_Lock();
    return ok;
}
static int flash_read(uint32_t address, void *dst, uint32_t len, void *ctx) {
    (void)ctx; memcpy(dst,(const void *)address,len); return 1;
}
static int store_boot(VcConfig *out) {
    const VsSlot slots[2]={
        {.address=0x080C0000u,.size=0x20000u,.erase=flash_erase,.program=flash_program,.read=flash_read,.ctx=NULL},
        {.address=0x080E0000u,.size=0x20000u,.erase=flash_erase,.program=flash_program,.read=flash_read,.ctx=NULL},
    };
    return vs_store_init(&store,slots,1,out);
}

static uint64_t now_us(void) {
    taskENTER_CRITICAL();
    uint32_t current=DWT->CYCCNT;
    mono_cycles+=(uint32_t)(current-mono_last_cycles); mono_last_cycles=current;
    uint64_t value=mono_cycles/(SystemCoreClock/1000000u);
    taskEXIT_CRITICAL(); return value;
}
uint64_t vehicle_time_us(void) { return now_us(); }

void vehicle_emergency_stop(void) {
    /* Also callable from priority 2/3 IRQs and fault hooks; no RTOS operation. */
    GPIOG->BSRR=(uint32_t)MOTOR_ARM_REQ_Pin<<16;
    TIM1->DIER&=~TIM_DIER_BIE;
    TIM1->BDTR&=~TIM_BDTR_MOE;
    TIM1->CCR1=0; TIM1->CCR2=0; TIM1->CCR3=0; TIM1->CCR4=0;
    urgent_fault=1;
}

static void rx_harvest(HostPort *p) {
    if (!p->uart->hdmarx || p->uart->RxState!=HAL_UART_STATE_BUSY_RX) return;
    uint16_t pos=(uint16_t)(sizeof(p->dma)-__HAL_DMA_GET_COUNTER(p->uart->hdmarx));
    if (pos==sizeof(p->dma)) pos=0;
    uint16_t old=p->position;
    p->produced+=(pos>=old ? (uint32_t)(pos-old) : (uint32_t)sizeof(p->dma)-old+pos);
    p->position=pos;
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size) {
    (void)size;
    for (unsigned i=0;i<2;i++) if (hosts[i].uart==uart) rx_harvest(&hosts[i]);
    vehicle_sensors_uart_rx(uart);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
    for (unsigned i=0;i<2;i++) if (hosts[i].uart==uart) hosts[i].errors++;
    vehicle_sensors_uart_error(uart);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart) {
    for (unsigned i=0;i<2;i++) if (hosts[i].uart==uart) hosts[i].tx_busy=0;
    vehicle_sensors_uart_tx(uart);
}
static int rx_start(HostPort *p) {
    p->position=0; p->produced=0; p->consumed=0;
    p->parser.used=0; p->parser.dropping=0;
    p->last_poll_ms=HAL_GetTick();
    return HAL_UARTEx_ReceiveToIdle_DMA(p->uart,p->dma,sizeof(p->dma))==HAL_OK;
}
static void rx_lost(unsigned src) {
    taskENTER_CRITICAL();
    if (mavlink_mode) vm_link_lost(&mavlink,src,now_us());
    else if (link.source==src && (link.arm_pending || link.state==VL_ARMING || link.state==VL_PI_ACTIVE || link.state==VL_WIFI_ACTIVE))
        vl_stop(&link,VL_FAULT,VL_FAULT_RX);
    taskEXIT_CRITICAL();
}

static void host_run(void *arg) {
    (void)arg;
    for (unsigned i=0;i<2;i++) if (!rx_start(&hosts[i])) hosts[i].errors++;
    for (;;) {
        for (unsigned src=0;src<2;src++) {
            HostPort *p=&hosts[src];
            uint32_t ms=HAL_GetTick();
            /* Poll at 1ms; an unobserved full wrap is ambiguous after 22ms.
             * Reject after 10ms scheduling gaps, even if the line was idle. */
            int gap=(uint32_t)(ms-p->last_poll_ms)>=10;
            p->last_poll_ms=ms;
            taskENTER_CRITICAL(); rx_harvest(p); uint32_t produced=p->produced; taskEXIT_CRITICAL();
            if (gap || p->errors!=p->seen_errors || produced-p->consumed>sizeof(p->dma) || p->uart->RxState!=HAL_UART_STATE_BUSY_RX) {
                rx_lost(src); p->parser.used=0; p->parser.dropping=1;
                p->consumed=produced; p->seen_errors=p->errors;
                /* Stop RX only; an in-flight TX owns a different DMA stream. */
                HAL_UART_AbortReceive(p->uart);
                if (!rx_start(p)) p->errors++;
                continue;
            }
            unsigned budget=512;
            while (p->consumed!=produced && budget--) {
                uint8_t byte=p->dma[p->consumed++%sizeof(p->dma)];
                if (mavlink_mode) {
                    uint64_t now=now_us();
                    taskENTER_CRITICAL();
                    rx_harvest(p);
                    if (p->produced-p->consumed>sizeof(p->dma)) {
                        taskEXIT_CRITICAL(); rx_lost(src); break;
                    }
                    vm_rx(&mavlink,src,&byte,1,now);
                    taskEXIT_CRITICAL();
                    continue;
                }
                VpFrame request, response;
                if (vp_feed(&p->parser,byte,&request)==1) {
                    uint64_t now=now_us();
                    taskENTER_CRITICAL();
                    rx_harvest(p);
                    if (p->produced-p->consumed>sizeof(p->dma)) {
                        taskEXIT_CRITICAL(); rx_lost(src); break;
                    }
                    vl_packet(&link,src,&request,now,&response);
                    taskEXIT_CRITICAL();
                    taskENTER_CRITICAL(); vt_push(&tx_pool,src,VT_CONTROL,&response); taskEXIT_CRITICAL();
                }
            }
        }
        host_progress++; vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static uint32_t physical_faults(void) {
    uint32_t result=health.fault_bits;
    if (!(GPIOG->IDR&ESTOP_OK_Pin) || (GPIOD->IDR&CHG_PRESENT_Pin) || !(GPIOD->IDR&PWR_ALERT_N_Pin)) result|=VL_FAULT_HARDWARE;
    return result;
}
static int interlock_ready(void) {
    return !(GPIOE->IDR&GPIO_PIN_15) && (GPIOG->IDR&0xf0)==0xf0;
}
static void safety_run(void *arg) {
    (void)arg;
    uint32_t last_control=0,last_host=0,last_tx=0,last_health=HAL_GetTick(),last_wdi=0;
    uint32_t last_sensor=0,last_imu=0,last_power=0;
    uint8_t wdi_low=0;
    /* Pi power is held independently on A. PD3 must stay low, no boot pulse. */
    HAL_GPIO_WritePin(AUX_PWR_EN_GPIO_Port,AUX_PWR_EN_Pin,GPIO_PIN_SET);
    HAL_GPIO_WritePin(MCU_WDI_GPIO_Port,MCU_WDI_Pin,GPIO_PIN_SET);
    for (;;) {
        uint64_t now=now_us(); uint32_t ms=HAL_GetTick();
        int pi_power_good=(PI_PWR_GOOD_GPIO_Port->IDR&PI_PWR_GOOD_Pin)!=0;
        HAL_GPIO_WritePin(LVL_SHIFT_OE_GPIO_Port,LVL_SHIFT_OE_Pin,pi_power_good ? GPIO_PIN_SET : GPIO_PIN_RESET);
        uint32_t primask=__get_PRIMASK(); __disable_irq();
        uint32_t fault=urgent_fault; urgent_fault=0;
        __set_PRIMASK(primask);
        taskENTER_CRITICAL();
        if (fault) vl_stop(&link,VL_FAULT,VL_FAULT_HARDWARE);
        vl_pi_inputs(&link, pi_power_good,
                     (PI_READY_GPIO_Port->IDR&PI_READY_Pin)!=0,
                     (PI_HALTED_GPIO_Port->IDR&PI_HALTED_Pin)!=0);
        int neutral=1;
        for (unsigned i=0;i<4;i++) if (fabsf(control.measured[i])>.5f) neutral=0;
        vl_tick(&link,now,physical_faults(),interlock_ready(),neutral);
        int wake=link.state==VL_ARMING || vl_permitted(&link,now);
        taskEXIT_CRITICAL();
        /* The final enable write and urgent-fault check must be indivisible. */
        primask=__get_PRIMASK(); __disable_irq();
        if (urgent_fault) wake=0;
        /* The A board intentionally asserts BKIN when ARM is removed.
         * Disable the break IRQ before that expected edge, while keeping the
         * hardware break path active whenever PWM is permitted. */
        if (!wake) { TIM1->DIER&=~TIM_DIER_BIE; TIM1->BDTR&=~TIM_BDTR_MOE; }
        HAL_GPIO_WritePin(DRV_WAKE_GPIO_Port,DRV_WAKE_Pin,wake ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_ARM_REQ_GPIO_Port,MOTOR_ARM_REQ_Pin,wake ? GPIO_PIN_SET : GPIO_PIN_RESET);
        __set_PRIMASK(primask);
        uint32_t sensor_progress, imu_progress;
        vehicle_sensors_progress(&sensor_progress,&imu_progress);
        if (control_progress!=last_control && host_progress!=last_host && tx_progress!=last_tx &&
            sensor_progress!=last_sensor && imu_progress!=last_imu && power_progress!=last_power) {
            last_control=control_progress; last_host=host_progress; last_tx=tx_progress; last_health=ms;
            last_sensor=sensor_progress; last_imu=imu_progress;
            last_power=power_progress;
        }
        if ((uint32_t)(ms-last_health)<50 && last_control && last_host && last_tx) {
            HAL_IWDG_Refresh(&hiwdg);
            if (wdi_low) { HAL_GPIO_WritePin(MCU_WDI_GPIO_Port,MCU_WDI_Pin,GPIO_PIN_SET); wdi_low=0; }
            if ((uint32_t)(ms-last_wdi)>=50) {
                HAL_GPIO_WritePin(MCU_WDI_GPIO_Port,MCU_WDI_Pin,GPIO_PIN_RESET); wdi_low=1; last_wdi=ms;
            }
        } else if ((uint32_t)(ms-last_health)>=50) vehicle_emergency_stop();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void power_run(void *arg) {
    (void)arg;
    TIM_TypeDef *timers[4]={TIM2,TIM3,TIM4,TIM5};
    uint32_t previous[4]={0}; int have_previous=0,previous_req=0,previous_cut=0;
    /* PD3 is a latch-clear clock, not a steady power-enable signal. */
    HAL_GPIO_WritePin(PI_PWR_CMD_GPIO_Port,PI_PWR_CMD_Pin,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PI_SHUTDOWN_REQ_GPIO_Port,PI_SHUTDOWN_REQ_Pin,GPIO_PIN_RESET);
    for (;;) {
        uint32_t ms=HAL_GetTick();
        int still=have_previous && control_progress!=0;
        for(unsigned i=0;i<4;i++) {
            uint32_t current=timers[i]->CNT;
            if(current!=previous[i]) still=0;
            previous[i]=current;
        }
        have_previous=1;
        int good=(PI_PWR_GOOD_GPIO_Port->IDR&PI_PWR_GOOD_Pin)!=0;
        int halted=(PI_HALTED_GPIO_Port->IDR&PI_HALTED_Pin)!=0;
        int off=!(MOTOR_ARM_REQ_GPIO_Port->ODR&MOTOR_ARM_REQ_Pin) && !(TIM1->BDTR&TIM_BDTR_MOE);
        /* Consume request and publish inhibit in one critical section. A
         * newly accepted host request must never be cleared by an idle tick. */
        taskENTER_CRITICAL();
        int request=link.shutdown_requested; link.shutdown_requested=0;
        vw_step(&pi_power,ms,request,good,halted,still,off);
        vl_power_update(&link,pi_power.inhibit);
        power_inputs=(uint16_t)((good ? 8u : 0u)|(halted ? 16u : 0u)|(still ? 32u : 0u)|(off ? 64u : 0u));
        int req_out=pi_power.request_out,cut_out=pi_power.cut_out;
        taskEXIT_CRITICAL();
        HAL_GPIO_WritePin(PI_SHUTDOWN_REQ_GPIO_Port,PI_SHUTDOWN_REQ_Pin,req_out ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PI_PWR_CMD_GPIO_Port,PI_PWR_CMD_Pin,cut_out ? GPIO_PIN_SET : GPIO_PIN_RESET);
        taskENTER_CRITICAL();
        vw_applied(&pi_power,HAL_GetTick(),previous_req,previous_cut);
        taskEXIT_CRITICAL();
        previous_req=req_out; previous_cut=cut_out;
        power_progress++; vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void encoder_start(TIM_HandleTypeDef *timer) {
    TIM_Encoder_InitTypeDef config={0};
    config.EncoderMode=TIM_ENCODERMODE_TI12;
    config.IC1Polarity=TIM_ICPOLARITY_RISING; config.IC2Polarity=TIM_ICPOLARITY_RISING;
    config.IC1Selection=TIM_ICSELECTION_DIRECTTI; config.IC2Selection=TIM_ICSELECTION_DIRECTTI;
    config.IC1Prescaler=TIM_ICPSC_DIV1; config.IC2Prescaler=TIM_ICPSC_DIV1;
    config.IC1Filter=4; config.IC2Filter=4;
    if (HAL_TIM_Encoder_Init(timer,&config)!=HAL_OK || HAL_TIM_Encoder_Start(timer,TIM_CHANNEL_ALL)!=HAL_OK) Error_Handler();
}
static void motor_write(int permitted) {
    uint32_t primask=__get_PRIMASK(); __disable_irq();
    if (!permitted || store_busy || urgent_fault || !interlock_ready() || physical_faults()) {
        TIM1->DIER&=~TIM_DIER_BIE;
        TIM1->CCR1=0; TIM1->CCR2=0; TIM1->CCR3=0; TIM1->CCR4=0; TIM1->BDTR&=~TIM_BDTR_MOE;
    } else {
        uint32_t direction=0, pulse[4];
        for (unsigned i=0;i<4;i++) {
            float duty=control.duty[i]*control.config.motor_sign[i];
            if (control.direction[i]*control.config.motor_sign[i]>0) direction|=1u<<i;
            pulse[i]=(uint32_t)(fabsf(duty)*(TIM1->ARR+1));
        }
        if ((GPIOG->ODR&0xf)!=direction) {
            /* Flush old PWM preload with MOE disabled before any DIR edge. */
            TIM1->BDTR&=~TIM_BDTR_MOE;
            TIM1->CCR1=0; TIM1->CCR2=0; TIM1->CCR3=0; TIM1->CCR4=0;
            TIM1->EGR=TIM_EGR_UG;
            GPIOG->BSRR=((~direction&0xf)<<16)|direction;
            __set_PRIMASK(primask); return;
        }
        /* Prevent a timer rollover from latching only part of this snapshot. */
        TIM1->CR1|=TIM_CR1_UDIS;
        TIM1->CCR1=pulse[0]; TIM1->CCR2=pulse[1]; TIM1->CCR3=pulse[2]; TIM1->CCR4=pulse[3];
        TIM1->CR1&=~TIM_CR1_UDIS;
        __HAL_TIM_CLEAR_FLAG(&htim1,TIM_FLAG_BREAK);
        __HAL_TIM_ENABLE_IT(&htim1,TIM_IT_BREAK);
        TIM1->BDTR|=TIM_BDTR_MOE;
    }
    __set_PRIMASK(primask);
}
static void control_run(void *arg) {
    (void)arg;
    encoder_start(&htim2); encoder_start(&htim3); encoder_start(&htim4); encoder_start(&htim5);
    TIM1->CCMR1|=TIM_CCMR1_OC1PE|TIM_CCMR1_OC2PE;
    TIM1->CCMR2|=TIM_CCMR2_OC3PE|TIM_CCMR2_OC4PE;
    TIM1->CR1|=TIM_CR1_ARPE;
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1); HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3); HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_4);
    TIM1->BDTR&=~TIM_BDTR_MOE;
    __HAL_TIM_CLEAR_FLAG(&htim1,TIM_FLAG_BREAK); __HAL_TIM_DISABLE_IT(&htim1,TIM_IT_BREAK);
    __HAL_TIM_CLEAR_FLAG(&htim6,TIM_FLAG_UPDATE);
    if (HAL_TIM_Base_Start_IT(&htim6)!=HAL_OK) Error_Handler();
    uint64_t previous=now_us();
    uint32_t start_ms=HAL_GetTick();
    for (;;) {
        uint32_t notifications=ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(10));
        uint64_t now=now_us();
        uint64_t period=now-previous;
        previous=now;
        float dt=period*1e-6f;
        uint32_t ms=HAL_GetTick();
        /* Boot-time task startup can legitimately be late by more than the
         * 2 ms control budget; only steady-state jitter is evidence. */
        int settling=(uint32_t)(ms-start_ms)<100;
        /* A parameter save briefly stalls instruction fetch from flash. The
         * vehicle is disarmed during the save, so suppress both the timing
         * fault and the jitter sample instead of latching VL_FAULT_CONTROL
         * or polluting the runtime evidence. */
        int maintenance=store_busy;
        if (maintenance && !store_programming && (int32_t)(ms-store_release_ms)>=0) {
            taskENTER_CRITICAL(); store_busy=0; link.maintenance=0; taskEXIT_CRITICAL();
            maintenance=0;
        }
        int64_t delta=(int64_t)period-1000;
        int64_t magnitude=delta<0 ? -delta : delta;
        if (!settling && !maintenance) {
            jitter_last_us=(uint32_t)magnitude;
            if ((uint32_t)magnitude>jitter_max_us) jitter_max_us=(uint32_t)magnitude;
        }
        uint32_t counts[4]={TIM2->CNT,TIM3->CNT,TIM4->CNT,TIM5->CNT};
        taskENTER_CRITICAL();
        if (!maintenance && !settling && (notifications!=1 || dt<.0005f || dt>.002f)) {
            control_overruns++; vl_stop(&link,VL_FAULT,VL_FAULT_CONTROL);
        }
        if (link.config_revision!=applied_revision) { vc_init(&control,&link.config); applied_revision=link.config_revision; }
        int permitted=vl_permitted(&link,now);
        vc_step(&control,counts,link.body,dt,permitted);
        VhInput h;
        memset(&h,0,sizeof(h));
        h.adc_valid=(uint8_t)vehicle_sensors_adc(h.adc);
        memcpy(h.wheel_rad_s,control.measured,sizeof(h.wheel_rad_s));
        memcpy(h.duty,control.duty,sizeof(h.duty));
        h.permitted=(uint8_t)(permitted!=0);
        vh_step(&health,&h,(dt>=.0005f && dt<=.002f) ? dt : .001f);
        motor_write(permitted);
        taskEXIT_CRITICAL();
        control_progress++;
    }
}
void vehicle_app_timer(TIM_HandleTypeDef *timer) {
    if (timer->Instance==TIM6 && control_task) {
        BaseType_t higher=pdFALSE; vTaskNotifyGiveFromISR(control_task,&higher); portYIELD_FROM_ISR(higher);
    }
}
void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *timer) { if (timer->Instance==TIM1) vehicle_emergency_stop(); }
void HAL_GPIO_EXTI_Callback(uint16_t pin) {
    vehicle_sensors_exti(pin);
    if (pin&(ESTOP_OK_Pin|PWR_ALERT_N_Pin)) vehicle_emergency_stop();
    if ((pin&(FAULT_FL_Pin|FAULT_FR_Pin|FAULT_RL_Pin|FAULT_RR_Pin)) && (GPIOG->ODR&MOTOR_ARM_REQ_Pin)) vehicle_emergency_stop();
}

int vehicle_send_telemetry(uint8_t type, const uint8_t *data, uint16_t len, uint64_t now, unsigned priority) {
    if (mavlink_mode) {
        /* Sensor tasks feed VP_IMU/VP_GNSS/VP_SBUS/VP_LIDAR here; the MAVLink
         * layer turns them into standard telemetry. Application-level VP
         * frames are not used in this mode. */
        taskENTER_CRITICAL();
        int result=vm_sensor(&mavlink,type,data,len,now);
        taskEXIT_CRITICAL();
        return result;
    }
    if (len>VP_PAYLOAD_MAX) return 0;
    VpFrame frame={0}; frame.type=type; frame.payload_len=len; frame.time_us=now;
    memcpy(frame.payload,data,len);
    taskENTER_CRITICAL(); frame.session=link.peer[VL_PI].session;
    int result=link.peer[VL_PI].established ? vt_push(&tx_pool,VL_PI,priority,&frame) : 0;
    taskEXIT_CRITICAL(); return result;
}

static void mavlink_step(uint64_t now) {
    VmInput in;
    VehicleStats stats;
    vehicle_app_stats(&stats);
    uint32_t sensors[12];
    vehicle_sensors_diagnostics(sensors);
    taskENTER_CRITICAL();
    memset(&in,0,sizeof(in));
    if (control.configured) vc_forward(&control.config,control.measured,in.body_measured);
    memcpy(in.wheel_measured,control.measured,sizeof(in.wheel_measured));
    memcpy(in.duty,control.duty,sizeof(in.duty));
    in.state=link.state; in.source=link.source; in.configured=control.configured;
    in.pi_ready=link.pi_ready; in.power_inhibit=link.power_inhibit;
    in.fault_bits=link.fault_bits;
    in.sensor_bits=sensors[0];
    in.error_imu=sensors[2]; in.error_gnss=sensors[4];
    in.error_lidar=sensors[6]; in.error_uart=sensors[7];
    in.rx_errors=hosts[0].errors+hosts[1].errors;
    in.control_overruns=control_overruns;
    in.tx_dropped=mavlink.tx_overflow[0]+mavlink.tx_overflow[1];
    in.battery_valid=health.voltage_valid;
    in.vbat_mv=health.vbat_mv; in.vmot_mv=health.vmot_mv;
    in.current_valid=health.current_valid;
    memcpy(in.current_a,health.current_a,sizeof(in.current_a));
    in.ntc_valid=health.ntc_valid; in.ntc_c=health.ntc_c;
    in.soc_valid=health.soc_valid; in.soc=health.soc;
    for (unsigned i=0;i<4;i++) if (health.stall[i]) in.stall_mask|=1u<<i;
    memcpy(in.stack_free,stats.stack_free,sizeof(in.stack_free));
    in.jitter_last_us=stats.jitter_last_us; in.jitter_max_us=stats.jitter_max_us;
    in.cpu_percent=stats.cpu_percent;
    in.config_seq=stats.config_seq; in.config_errors=stats.config_errors;
    vm_tick(&mavlink,&in,now);
    taskEXIT_CRITICAL();
}

static void mavlink_pump(uint32_t ms) {
    for (unsigned i=0;i<2;i++) {
        HostPort *p=&hosts[i];
        if (p->tx_busy && (uint32_t)(ms-p->tx_started)>20) {
            HAL_UART_AbortTransmit(p->uart); p->tx_busy=0; vm_tx_drop(&mavlink,i);
        }
        if (p->tx_busy) continue;
        uint16_t n;
        taskENTER_CRITICAL(); n=vm_tx_pop(&mavlink,i,p->tx_wire,sizeof(p->tx_wire)); taskEXIT_CRITICAL();
        if (!n) continue;
        p->tx_busy=1; p->tx_started=ms;
        if (HAL_UART_Transmit_DMA(p->uart,p->tx_wire,n)!=HAL_OK) { p->tx_busy=0; vm_tx_drop(&mavlink,i); }
    }
}

static void config_store_step(uint64_t now) {
    uint8_t dirty, state;
    uint64_t since;
    uint32_t revision;
    VcConfig config;
    taskENTER_CRITICAL();
    dirty=link.config_dirty; state=link.state; since=link.config_dirty_since_us;
    revision=link.config_revision; config=link.config;
    if (store_busy || link.arm_pending || !dirty || state!=VL_DISARMED || now<since ||
        now-since<UINT64_C(750000) || !vc_config_valid(&config)) {
        taskEXIT_CRITICAL(); return;
    }
    /* Acquire the maintenance inhibit atomically with the DISARMED snapshot.
     * ARM handlers share this critical section and reject while it is held. */
    link.maintenance=1;
    store_programming=1; store_busy=1;
    GPIOG->BSRR=(uint32_t)MOTOR_ARM_REQ_Pin<<16;
    TIM1->BDTR&=~TIM_BDTR_MOE;
    taskEXIT_CRITICAL();
    int ok=vs_store_save(&store,&config,link.boot_id);
    store_release_ms=HAL_GetTick()+20;
    store_programming=0;
    taskENTER_CRITICAL();
    if (ok) {
        link.config_store_seq=vs_store_seq(&store);
        if (link.config_revision==revision) link.config_dirty=0;
    } else {
        link.config_store_errors++;
        if (link.config_revision==revision) link.config_dirty=0; /* Retry only on a new edit. */
    }
    taskEXIT_CRITICAL();
}
static void cpu_sample(void) {
    uint32_t idle=ulTaskGetIdleRunTimeCounter(), cycles=vehicle_runtime_counter();
    if (stats_have) {
        uint32_t idle_delta=idle-stats_idle_last, cycle_delta=cycles-stats_cycles_last;
        if (cycle_delta) {
            uint32_t idle_pct=(uint32_t)((uint64_t)idle_delta*100u/cycle_delta);
            cpu_percent=(uint8_t)(idle_pct>=100u ? 0u : 100u-idle_pct);
        }
    }
    stats_idle_last=idle; stats_cycles_last=cycles; stats_have=1;
}
void vehicle_app_stats(VehicleStats *out) {
    if (!out) return;
    memset(out,0,sizeof(*out));
    out->stack_free[0]=(uint16_t)uxTaskGetStackHighWaterMark(safety_task);
    out->stack_free[1]=(uint16_t)uxTaskGetStackHighWaterMark(control_task);
    out->stack_free[2]=(uint16_t)uxTaskGetStackHighWaterMark(host_task);
    out->stack_free[3]=(uint16_t)uxTaskGetStackHighWaterMark(telemetry_task);
    out->stack_free[4]=(uint16_t)uxTaskGetStackHighWaterMark(power_task);
    uint16_t sensor[2]={0,0};
    vehicle_sensors_stack(sensor);
    out->stack_free[5]=sensor[0]; out->stack_free[6]=sensor[1];
    out->jitter_last_us=jitter_last_us; out->jitter_max_us=jitter_max_us;
    out->cpu_percent=cpu_percent;
    out->config_seq=vs_store_seq(&store); out->config_errors=vs_store_errors(&store);
}

static void telemetry_run(void *arg) {
    (void)arg;
    uint32_t last_odom=0,last_status=0,last_cpu=0;
    for (;;) {
        uint32_t ms=HAL_GetTick(); uint64_t now=now_us();
        config_store_step(now);
        if ((uint32_t)(ms-last_cpu)>=1000) { cpu_sample(); last_cpu=ms; }
        if (mavlink_mode) {
            mavlink_step(now);
            mavlink_pump(ms);
            tx_progress++; vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if ((uint32_t)(ms-last_odom)>=10) {
            uint8_t p[64]={0}; vp_put_u64(p,now);
            taskENTER_CRITICAL();
            for (unsigned i=0;i<4;i++) { vp_put_u64(p+8+8*i,(uint64_t)control.total[i]); vp_put_u32(p+40+4*i,(uint32_t)(int32_t)(control.measured[i]*1000)); }
            vp_put_u32(p+56,link.fault_bits); vp_put_u16(p+60,control.configured ? 0xf : 0);
            taskEXIT_CRITICAL(); vehicle_send_telemetry(VP_ODOM,p,sizeof(p),now,VT_PERIODIC); last_odom=ms;
        }
        if ((uint32_t)(ms-last_status)>=100) {
            uint8_t p[32]={0}; vp_put_u64(p,now);
            vp_put_u16(p+8,UINT16_MAX); vp_put_u16(p+10,UINT16_MAX);
            uint16_t vbat=0,vm=0;
            if (vehicle_sensors_voltages(&vbat,&vm)) { vp_put_u16(p+8,vbat); vp_put_u16(p+10,vm); }
            vp_put_u32(p+12,INT32_MAX); vp_put_u16(p+16,UINT16_MAX); vp_put_u16(p+18,0x8000);
            taskENTER_CRITICAL(); vp_put_u32(p+20,link.fault_bits); p[24]=link.state; p[25]=link.source;
            uint64_t age=link.last_command_us ? (now-link.last_command_us)/1000 : UINT16_MAX;
            vp_put_u16(p+28,(uint16_t)(age>UINT16_MAX ? UINT16_MAX : age)); taskEXIT_CRITICAL();
            uint16_t io=VP_IO_VALID;
            if (HAL_GPIO_ReadPin(PI_READY_GPIO_Port,PI_READY_Pin)==GPIO_PIN_SET) io|=VP_IO_PI_READY;
            if (HAL_GPIO_ReadPin(PI_HALTED_GPIO_Port,PI_HALTED_Pin)==GPIO_PIN_SET) io|=VP_IO_PI_HALTED;
            if (HAL_GPIO_ReadPin(PI_PWR_GOOD_GPIO_Port,PI_PWR_GOOD_Pin)==GPIO_PIN_SET) io|=VP_IO_PI_POWER_GOOD;
            if (LVL_SHIFT_OE_GPIO_Port->ODR&LVL_SHIFT_OE_Pin) io|=VP_IO_LEVEL_SHIFT_REQUESTED;
            if (PI_SHUTDOWN_REQ_GPIO_Port->ODR&PI_SHUTDOWN_REQ_Pin) io|=VP_IO_SHUTDOWN_REQUESTED;
            vp_put_u16(p+30,io);
            vp_put_u16(p+26,UINT16_MAX); vehicle_send_telemetry(VP_STATUS,p,sizeof(p),now,VT_PERIODIC); last_status=ms;
            uint32_t sensors[12]; uint8_t diagnostic[64]={0};
            vehicle_sensors_diagnostics(sensors); vp_put_u64(diagnostic,now);
            for (unsigned k=0;k<8;k++) vp_put_u32(diagnostic+8+4*k,sensors[k]);
            taskENTER_CRITICAL();
            vp_put_u32(diagnostic+40,tx_pool.dropped); vp_put_u32(diagnostic+44,tx_pool.coalesced);
            vp_put_u32(diagnostic+56,hosts[0].errors+hosts[1].errors); vp_put_u32(diagnostic+60,control_overruns);
            taskEXIT_CRITICAL();
            vp_put_u32(diagnostic+48,sensors[8]); vp_put_u32(diagnostic+52,sensors[9]);
            vehicle_send_telemetry(VP_DIAGNOSTICS,diagnostic,sizeof(diagnostic),now,VT_PERIODIC);
            uint8_t power[16]={0}; vp_put_u64(power,now);
            taskENTER_CRITICAL();
            power[8]=pi_power.state; power[9]=pi_power.fault;
            vp_put_u16(power+10,(uint16_t)(power_inputs|(pi_power.request_out ? 1u : 0u)|
                          (pi_power.cut_out ? 2u : 0u)|(link.power_inhibit ? 4u : 0u)));
            vp_put_u32(power+12,(uint32_t)(ms-pi_power.stage_since_ms));
            taskEXIT_CRITICAL();
            vehicle_send_telemetry(VP_POWER_STATUS,power,sizeof(power),now,VT_PERIODIC);
        }
        for (unsigned i=0;i<2;i++) {
            HostPort *p=&hosts[i];
            if (p->tx_busy && (uint32_t)(ms-p->tx_started)>20) { HAL_UART_AbortTransmit(p->uart); p->tx_busy=0; }
            if (!p->tx_busy) {
                VpFrame frame;
                taskENTER_CRITICAL();
                int ready=vt_pop(&tx_pool,i,&frame);
                if (ready) frame.seq=++link.tx_seq; /* Sequence follows actual priority-ordered transmission. */
                taskEXIT_CRITICAL();
                if (ready) {
                    size_t n=vp_encode(&frame,p->tx_wire,sizeof(p->tx_wire));
                    if (n) {
                        p->tx_busy=1; p->tx_started=ms;
                        if (HAL_UART_Transmit_DMA(p->uart,p->tx_wire,(uint16_t)n)!=HAL_OK) p->tx_busy=0;
                    }
                }
            }
        }
        tx_progress++; vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task; (void)name; vehicle_emergency_stop(); __disable_irq(); for (;;) {}
}
void vApplicationMallocFailedHook(void) { vehicle_emergency_stop(); __disable_irq(); for (;;) {} }

void vehicle_app_init(void) {
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0; DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    /* PLLQ supplies 45MHz. Hardware random boot seed prevents replaying a
     * previous boot's token transcript. This is not network authentication. */
    RNG_HandleTypeDef rng={0}; rng.Instance=RNG;
    __HAL_RCC_RNG_CLK_ENABLE();
    uint32_t boot_id=0;
    if (HAL_RNG_Init(&rng)!=HAL_OK || HAL_RNG_GenerateRandomNumber(&rng,&boot_id)!=HAL_OK || !boot_id) Error_Handler();
    HAL_RNG_DeInit(&rng); __HAL_RCC_RNG_CLK_DISABLE();
    vl_init(&link,boot_id);
    link.mavlink=(uint8_t)(mavlink_mode!=0);
    /* Load the last persisted PARAM image before the control task exists, so
     * the vehicle is armable without the Pi resending calibration. Sector
     * erases only happen here, before MX_IWDG_Init(); SRAM polling also services the always-on
     * external TPS3431 while motors remain inhibited. */
    VcConfig stored;
    if (store_boot(&stored) && vc_config_valid(&stored)) {
        link.config=stored;
        link.configured=1;
        link.config_revision=1;
    }
    link.config_store_seq=vs_store_seq(&store);
    vc_init(&control,link.configured ? &link.config : NULL);
    vh_init(&health);
    if (mavlink_mode) vm_init(&mavlink,&link,now_us());
    hosts[VL_PI].uart=&huart6; hosts[VL_WIFI].uart=&huart1;
    safety_task=xTaskCreateStatic(safety_run,"Safety",512,NULL,15,safety_stack,&safety_tcb); configASSERT(safety_task);
    control_task=xTaskCreateStatic(control_run,"Control",1024,NULL,14,control_stack,&control_tcb); configASSERT(control_task);
    host_task=xTaskCreateStatic(host_run,"HostRx",1536,NULL,11,host_stack,&host_tcb); configASSERT(host_task);
    telemetry_task=xTaskCreateStatic(telemetry_run,"Telemetry",1024,NULL,7,telemetry_stack,&telemetry_tcb); configASSERT(telemetry_task);
    power_task=xTaskCreateStatic(power_run,"Power",512,NULL,6,power_stack,&power_tcb); configASSERT(power_task);
    vehicle_sensors_init();
}
