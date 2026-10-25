> 2026-10-25 归档复核：64 个必需引脚匹配。PD8/PC10/PC11/PE0/PE1 在当前 PCB2 无铜网络，分别对应 LD06_TX、POSE_TX/RX、LOG_RX/TX，仅为软件预留，不能用于现板外接。详细机器记录见 `../reports/hardware_contract_v1.json`。

# B_Control（STM32F429 · LQFP144）引脚定义 v1

来源：`doc/麦克纳姆无人车双板硬件与FreeRTOS设计说明_v1.docx` §4.2/§4.3/§5.1~§5.3。
物理脚号：`doc/datasheet/STM32F429ZGT6_C46525.pdf` 表 10 逐行提取（144 脚中 136 脚由 PDF 文本直接解析，其余 8 个为被文本行覆盖的 VDD/VSS 供电脚，按排列规律补全）。
机器可读版本与全脚表：`Code/cubemx/B板引脚分配_v1.csv`、`Code/docs/LQFP144_全脚定义_v1.md`。

## 1 电机与驱动（TIM1 + GPIO）

| 功能 | 引脚 | 物理脚 | 复用 / AF | 方向 |
|---|---|---|---|---|
| PWM_FL / FR / RL / RR | PE9 / PE11 / PE13 / PE14 | 60 / 64 / 66 / 67 | TIM1_CH1~CH4, AF1 | 输出，20 kHz |
| 驱动禁止 MOTOR_INHIBIT | PE15 | 68 | TIM1_BKIN, AF1 | 输入，高有效，B 端上拉 |
| DIR_FL / FR / RL / RR | PG0 / PG1 / PG2 / PG3 | 56 / 57 / 87 / 88 | GPIO 输出 | 输出，同端口 BSRR 原子写 |
| FAULT_FL / FR / RL / RR | PG4 / PG5 / PG6 / PG7 | 89 / 90 / 91 / 92 | GPIO_EXTI4 / EXTI5~7 | 输入，10 k 上拉 |
| 驱动唤醒 DRV_WAKE | PD14 | 85 | GPIO 输出 | 输出，默认低 |

DIR 全部落在 G 口：一次 `GPIOG->BSRR` 写 `((~n)&0xF)<<16 | (n&0xF)` 即可同时更新四路方向。四个 FAULT 也同口，`GPIOG->IDR & 0x00F0` 一次读完。

## 2 四路正交编码器

| 编码器 | 定时器 | 引脚 | 物理脚 | 位宽 |
|---|---|---|---|---|
| 左前 FL | TIM2 | PA15 / PB3 | 110 / 133 | 32 位 |
| 右前 FR | TIM3 | PA6 / PA7 | 42 / 43 | 16 位 |
| 左后 RL | TIM4 | PB6 / PB7 | 136 / 137 | 16 位 |
| 右后 RR | TIM5 | PA0 / PA1 | 34 / 35 | 32 位 |

- PA15 = JTDI、PB3 = JTDO：必须关闭 JTAG、只留 SWD，否则这两脚不能作 TIM2 编码器输入，SWO 也不再可用。
- 编码器差分计数在 16 位定时器上要做回绕差分（`int16_t)(now - last)`）。
- A 板用 4 颗 SN74LVC2G17 把 8 路整形成 3.3 V 后送 B；5 V 推挽输出不得直连 MCU 脚。

## 3 串口（7 路使用，UART5 保留）

| 功能 | 引脚 | 物理脚 | 复用 | 线格式 | 接收方式 |
|---|---|---|---|---|---|
| ESP32 | PA9 / PA10 | 101 / 102 | USART1 AF7 | 921600 8N1 | DMA2 Stream2 Ch4（RX）、Stream7 Ch4（TX） |
| GNSS | PD5 / PD6 | 119 / 122 | USART2 AF7 | 115200 8N1 | DMA1 Stream5 Ch4 |
| LD06 雷达 | PD9（PD8 仅测试点） | 78（77） | USART3 AF7 | 230400 8N1，只接收 | DMA1 Stream1 Ch4 |
| 位姿口 J_POSE | PC10 / PC11 | 111 / 112 | UART4 AF8 | 按模块 115200~921600 | DMA1 Stream2 Ch4 |
| 树莓派 | PG14 / PG9 | 129 / 124 | USART6 AF8 | 921600 8N1 | DMA2 Stream1 Ch5（RX）、Stream6 Ch5（TX） |
| SBUS | PF6 | 18 | UART7_RX AF8 | 100000，9B + EVEN + 2STOP | DMA1 Stream3 **Ch5** |
| 本地日志 | PE1 / PE0 | 142 / 141 | UART8 AF8 | 115200 8N1，限速 | 中断收发，不用 DMA |

波特率误差（过采样 16、带小数分频，见时钟文档）：USART1/USART6 的 921600 为 +0.03%，UART4 的 921600 为 +0.03%，USART2/USART3/UART8/UART7 为**精确值**。基线里写的“实际约 918367 baud”是把分频系数取整到 98 才会出现的 0.35% 偏差，带小数分频时不会那么差。

## 4 模拟量（ADC1，7 个规则通道 + DMA）

| 功能 | 引脚 | 物理脚 | 通道 | 采样时间 | 备注 |
|---|---|---|---|---|---|
| 四轮电流 I_FL/FR/RL/RR | PC0 / PC1 / PC2 / PC3 | 26 / 27 / 28 / 29 | IN10~IN13 | 56 周期 | A 板 STSPIN948 的 VA/VB 经 100~330 Ω + RC |
| 电池电压 VBAT_DIV | PC4 | 44 | IN14 | 56 周期 | 120 k / 10 k 分压，非直连母线 |
| 母线电压 VM_DIV | PC5 | 45 | IN15 | 56 周期 | 同上 |
| 温度 NTC | PB1 | 47 | IN9 | 144 周期 | A 板 10 kΩ 上拉、100 Ω 串联、100 nF；外接 NTC 类型待确认 |

转换触发用 **TIM8 TRGO（20 kHz，与 PWM 同频但尚未实现硬件相位同步）**，DMA2 Stream0 循环搬运。ADC 时钟 = PCLK2/4 = 22.5 MHz；7 通道一次扫描约 25 µs，占 20 kHz 周期（50 µs）的一半以内。若要留更多裕量，可把 VBAT/VM/NTC 改为降频转换（TIM8 的重复计数器分频），电流四路保持每周期一次。

## 5 IMU（ICM-42688-P，SPI2）

| 信号 | 引脚 | 物理脚 | 复用 |
|---|---|---|---|
| SCK | PB13 | 74 | SPI2_SCK AF5 |
| MISO | PB14 | 75 | SPI2_MISO AF5 |
| MOSI | PB15 | 76 | SPI2_MOSI AF5 |
| CS（软件片选） | PB12 | 73 | GPIO 输出，**上电应为高** |
| DRDY | PD2 | 116 | GPIO_EXTI2 |

SPI2 挂在 APB1，时钟 = 45 MHz / 4 = 11.25 MHz（ICM-42688-P 上限 24 MHz，留裕量；BR=/8 只有 5.625 MHz）。
SPI2 的 RX 默认走 DMA1 Stream3 Ch0，而该流已给 SBUS（Ch5），**同一个流不同通道不能并存**，所以 IMU 的短事务用限时轮询或中断，不占 DMA。

## 6 板间电源管理与人机

| 功能 | 引脚 | 物理脚 | 说明 |
|---|---|---|---|
| 板间 I²C1 SCL / SDA | PB8 / PB9 | 139 / 140 | I²C1 AF4，100 kHz 起，开漏 + 4.7 k 上拉，PowerTask 独占 |
| 驱动许可请求 MOTOR_ARM_REQ | PG10 | 125 | 输出，A 板外部下拉 |
| 急停状态 ESTOP_OK | PG11 | 126 | 输入，EXTI11 |
| 外部看门狗 MCU_WDI | PG12 | 127 | 输出，约 50 ms 一个下降沿 |
| GNSS PPS | PG8 | 93 | 输入，EXTI8，与 PG5~PG7 共享 EXTI9_5 |
| LD06 外控调速 | PE5 | 4 | TIM9_CH1 AF3，PWM 30 kHz（默认焊桥接地，选中外控时才有效） |
| LD06 局部供电 | PD13 | 82 | GPIO 输出，默认低；TPS22918/TPS2553 支路使能 |
| LD06 支路故障 | PD11 | 80 | GPIO 输入上拉，仅轮询；不启用 EXTI11，保留 PG11 急停 |
| Pi 电源命令 PI_PWR_CMD | PD3 | 117 | PowerTask 输出，默认低；上升沿清 A 保持锁存，仅在内核 HALTED 后发脉冲 |
| 外设轨使能 AUX_PWR_EN | PD4 | 118 | 输出 |
| Pi 供电状态 PI_PWR_GOOD | PC7 | 97 | 输入，轮询 |
| 外设轨状态 AUX_PWR_GOOD | PC8 | 98 | 输入，轮询 |
| 充电接入 CHG_PRESENT | PD7 | 123 | 输入，轮询；充电时禁止解锁 |
| 电源告警 PWR_ALERT_N | PD10 | 79 | 输入，EXTI10 |
| Pi 就绪 PI_READY | PD0 | 114 | 输入，来自 Pi GPIO17 |
| Pi 已停机 PI_HALTED | PD1 | 115 | 输入，来自 Pi GPIO22 |
| 请求 Pi 关机 PI_SHUTDOWN_REQ | PC6 | 96 | 输出，高有效；经 AO3400A 拉低 Pi GPIO27 |
| 电平转换器使能 LVL_SHIFT_OE | PD12 | 81 | 输出，TXU0202/TXU0102 的 OE |

## 7 调试与最小系统

| 项 | 引脚 | 物理脚 |
|---|---|---|
| SWDIO / SWCLK | PA13 / PA14 | 105 / 109 |
| 复位 | NRST | 25（A 板看门狗 WDO 也接此脚） |
| 启动 | BOOT0 | 138（10 k 下拉 + 跳线） |
| PDR_ON | PDR_ON | 143（接 VDD） |
| HSE 8 MHz | PH0-OSC_IN / PH1-OSC_OUT | 23 / 24（晶体 + 15~18 pF，底部禁走线） |
| VBAT | VBAT | 6（接 3.3 V；**绝不能接 4S**） |
| VCAP1 / VCAP2 | VCAP_1 / VCAP_2 | 71 / 106（各 2.2 µF 低 ESR） |
| VDDA / VREF+ / VSSA | VDDA / VREF+ / VSSA | 33 / 32 / 31（磁珠或 10 Ω + 100 nF/1 µF） |

## 8 DMA 分配（与基线的坑）

| 控制器 | Stream | Channel | 用途 | 模式 |
|---|---|---|---|---|
| DMA1 | S1 | 4 | USART3_RX（LD06） | 循环 |
| DMA1 | S2 | 4 | UART4_RX（位姿） | 循环 |
| DMA1 | S3 | **5** | UART7_RX（SBUS） | 循环，Very High |
| DMA1 | S5 | 4 | USART2_RX（GNSS） | 循环 |
| DMA2 | S0 | 0 | ADC1 规则序列（16 位元素） | 循环 |
| DMA2 | S1 | 5 | USART6_RX（Pi） | 循环 |
| DMA2 | S2 | 4 | USART1_RX（ESP32） | 循环 |
| DMA2 | S6 | 5 | USART6_TX（Pi） | 单次 |
| DMA2 | S7 | 4 | USART1_TX（ESP32） | 单次 |

- F4 上“流 + 通道”的组合决定请求映射，所以通道号必须与上表一致。
- **所有 RX 环必须放在 0x2000_0000 的 SRAM1/2/3**；0x1000_0000 的 64 KB CCM 只给 CPU（DMA 访问不到），可放任务栈与 CPU 私有状态。
- 循环 DMA 同时用 IDLE、半传输 HT、完成 TC 三种事件；HAL 回调给出的 Size 是**缓冲位置**语义，要按上次位置算增量并处理回绕；IDLE 只表示线路空闲，LD06 这种连续流不能只靠 IDLE 切帧。

## 9 J_AB_SIG 2×30 板间信号网表（60 针）

奇数针 / 偶数针成对读；箭头表示该网名对应哪一侧的 MCU 引脚。

| 奇 | 网络 → MCU | 偶 | 网络 → MCU |
|---|---|---|---|
| 1 | PWM_FL → PE9 | 2 | GND |
| 3 | DIR_FL → PG0 | 4 | FAULT_FL → PG4 |
| 5 | PWM_FR → PE11 | 6 | GND |
| 7 | DIR_FR → PG1 | 8 | FAULT_FR → PG5 |
| 9 | PWM_RL → PE13 | 10 | GND |
| 11 | DIR_RL → PG2 | 12 | FAULT_RL → PG6 |
| 13 | PWM_RR → PE14 | 14 | GND |
| 15 | DIR_RR → PG3 | 16 | FAULT_RR → PG7 |
| 17 | ENC_FL_A → PA15 | 18 | ENC_FL_B → PB3 |
| 19 | GND | 20 | ENC_FR_A → PA6 |
| 21 | ENC_FR_B → PA7 | 22 | GND |
| 23 | ENC_RL_A → PB6 | 24 | ENC_RL_B → PB7 |
| 25 | GND | 26 | ENC_RR_A → PA0 |
| 27 | ENC_RR_B → PA1 | 28 | GND |
| 29 | MOTOR_ARM_REQ → PG10 | 30 | ESTOP_OK → PG11 |
| 31 | MCU_WDI → PG12 | 32 | DRV_WAKE → PD14 |
| 33 | PI_PWR_CMD → PD3 | 34 | PI_PWR_GOOD → PC7 |
| 35 | AUX_PWR_EN → PD4 | 36 | AUX_PWR_GOOD → PC8 |
| 37 | I2C1_SCL → PB8 | 38 | I2C1_SDA → PB9 |
| 39 | I_FL → PC0 | 40 | GND |
| 41 | I_FR → PC1 | 42 | GND |
| 43 | I_RL → PC2 | 44 | GND |
| 45 | I_RR → PC3 | 46 | GND |
| 47 | VBAT_DIV → PC4 | 48 | VM_DIV → PC5 |
| 49 | NTC → PB1 | 50 | MOTOR_INHIBIT → PE15 |
| 51 | CHG_PRESENT → PD7 | 52 | PWR_ALERT_N → PD10 |
| 53 | GND | 54 | GND |
| 55 | WATCHDOG_RST_N → NRST | 56 | SPARE2（未装跳线） |
| 57 | GND | 58 | GND |
| 59 | SPARE3（未装跳线） | 60 | GND |

方向说明：B→A 输出为 PWM、DIR、ARM、WDI、DRV_WAKE、PI_PWR_CMD、AUX_PWR_EN；编码器、FAULT、四路电流、分压、PGOOD、ESTOP、CHG_PRESENT、PWR_ALERT_N、MOTOR_INHIBIT 由 A→B；I²C 按开漏双向。
针 55 是 A 板看门狗的**开漏**输出，接到 B 的 NRST 上，不是普通 GPIO。

## 10 J_AB_PWR 8 针

| 针 | 网络 | 容量 |
|---|---|---|
| 1、2 | 5V_PI（约 5.1 V） | 5 A 轨，两针并联同长度 |
| 3、4 | GND_PI | 回流，两针并联 |
| 5、6 | 5V_AUX / GND | 外设、SBUS、编码器，3 A |
| 7、8 | 3V3_CTRL / GND | MCU、ESP32、逻辑常供，2 A |

电池与电机大电流不经此接口，也不经 60 针信号座。A/B 同一供电域，装拆要断电。

## 11 B 与树莓派 40 针

| Pi 物理针 | BCM | 连接 |
|---|---|---|
| 2、4 | 5V | 5V_PI（约 5.1 V），低阻铜 |
| 6、9、14、20、25、30、34、39 | GND | 全部接地 |
| 8 | GPIO14 / TXD0 | Pi TX → 电平转换器 → USART6_RX（PG9） |
| 10 | GPIO15 / RXD0 | USART6_TX（PG14）→ 电平转换器 → Pi RX |
| 11 | GPIO17 | PI_READY → PD0，高有效 |
| 13 | GPIO27 | PI_SHUTDOWN_REQ_N ← PC6 经 AO3400A 开漏 |
| 15 | GPIO22 | PI_HALTED → PD1，高有效 |
| 1、17 | Pi 3.3 V | 仅作 Pi 侧电源参考，不与 3V3_CTRL 并联 |
| 27、28 | ID_SD / ID_SC | 不实现 HAT EEPROM，不接 |
| 其余 | — | 保留 |

UART 用 TXU0202（VCCA=3V3_CTRL、VCCB=PI_3V3，OE=PD12，外部下拉默认禁止），PI_READY/PI_HALTED 用同一 OE 控制的 TXU0102。Pi 侧需把物理 8/10 固定到 PL011 并关闭该串口控制台。

## 12 预留脚与不可占用说明

- 预留（未分配）：PA2、PA3、PA4、PA5、PA8、PA11、PA12、PB0、PB2、PB4、PB5、PB10、PB11、PC9、PC12、PC13、PC14、PC15、PD15、PE2、PE3、PE7、PE8、PE10、PE12、PF0~PF5、PF7~PF15、PG13、PG15。
- PA0/PA1 已给 TIM5 编码器，**不能再**分配给待机唤醒、ADC 或电压监测；旧 `PCB/bom/引脚分配_v1.csv` 里的 PC0~PC3 电压/温度方案也已作废。
- PB3/PA15 需要关 JTAG；SWO 不可用（调试只用 SWD 四线 + 复位）。
- PD8 只作测试点，不接连接器；LD06 是只接收方向。
- PE15（BKIN）与 PA6（TIM3_CH1）、PB12（IMU_CS）互斥，后两者已占用，所以 BKIN 只能在 PE15。
- PC14/PC15 若装 32.768 kHz LSE 则归 RTC，否则保持预留。
- PA11/PA12 只有在改用 168 MHz 时钟、启用 USB OTG FS 时才启用。

Pi 电源软件已接入静态 PowerTask：PC6、PD3 为唯一写者；PC7/PD1 为实际输入反馈。程序和时序见 `Pi有序关机与切电_v1.md`。表中板间定义仍须补齐实际 A/B 连接器及 PCB 接线，不能把引脚表当作已完成连通的证据。

A 板外接 J6 已修正为 VM 限流的无源 NC 触点回路，不能接 GPIO 或把其中一针当 GND。光耦/施密特输出到 B 的 ESTOP_OK 仍为高有效；具体接法见 `PCB/docs/A板急停回路修正核验_v1.md`。

2026-10-23 外接天线模块更新：U29=ESP32-WROOM-32UE-N4；STM32 PA9/TX 经 R73 接模块 GPIO16/U2RX，PA10/RX 经 R74 接 GPIO17/U2TX，921600 8N1。PE4=ESP_EN、PE6=ESP_BOOT，均开漏默认释放；PE5 保持 LD06 PWM。模块 UART0 单独接 TP32/TX、TP33/RX，用于烧录和日志。
