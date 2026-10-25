# MecanumWheelvehicle

四轮麦克纳姆轮车的机械、电路、STM32 / FreeRTOS 固件、ESP32 无线桥与 Raspberry Pi / ROS 2 工程。**此仓库只发布最终交付文件；过程快照、旧备份和编译缓存保留在开发机，不上传。**车辆采用独立摆臂悬挂，控制器为 STM32F429ZGT6，电机驱动采用 STSPIN948，主通信协议为 MAVLink v2。

**当前状态（2026-10-25）**：软件回归 **80 项通过**，STM32 全量构建零告警；归档 PCB2 的 **64 个必需引脚**、A/B **60 位信号接口和 8 位电源接口**核对通过。PCB3_1 已完成此前的几何间距与连通性复核。尚无实板烧录、ADC 标定、HIL、温升和运行负载实测，当前版本用于台架联调，不能据此认定整车已通过工业验收。

## 先读哪些文件

| 入口 | 用途 |
| --- | --- |
| [代码与电路一致性复核](Code/docs/代码与电路一致性复核_v1.md) | 本轮修复、硬件依据、验证命令与剩余问题 |
| [FreeRTOS 程序报告](Code/docs/FreeRTOS程序报告_v1.html) | 可离线打开的详细报告：任务、启动、协议、保护、引脚、部署和证据 |
| [完整文件索引](FILES.md) | 逐文件路径、分类及用途，覆盖发布文件；上游库按原目录保留 |
| [固件开发说明](Code/README.md) | STM32、ROS 2、传感器与测试入口 |
| [PCB3_1 布线完成复核](PCB/docs/PCB3_1_布线完成复核_v1.md) | 最新 A 板 PCB3_1 的连通性、间距、覆铜和加工要求 |
| [贡献约定](AGENTS.md) | CAD 命名、脚本操作约定；其中 Code 为空等描述属于早期状态，以当前 README 和复核报告为准 |

浏览器本地打开 HTML，或启动只读文档服务：

```bash
python3 -m http.server 8000 --bind 127.0.0.1
# 打开 http://127.0.0.1:8000/Code/docs/FreeRTOS程序报告_v1.html
```

## 系统组成与工程基线

- **A 电源/驱动板**：当前布线对象名为 `PCB3_1`；负责四路电机驱动、电流与电压采集接口、电源保持、急停和外部看门狗。
- **B 控制板**：当前对象名为 `PCB2`；STM32F429ZGT6 执行 1 kHz 四轮速度闭环、故障联锁和传感器采集，通过 USART6 连接 Pi、USART1 连接 ESP32。
- **Pi**：运行 ROS 2 Humble 的 `mecanum_mavlink`，管理串口、运动服务、遥测、GPIO17 READY。GPIO22 HALTED 必须由内核产生，GPIO27 接关机请求。
- **ESP32**：MAVLink UART ↔ Wi-Fi UDP 桥；无线链路不替代本地急停和驱动联锁。默认示例 AP 密码只用于开发，部署前修改。
- **机械**：SolidWorks 装配为机械源文件；STEP 用于跨工具交换，Blender 场景用于布局与维护空间检查。本次只发布当前提供的 `Vehicle Layout v15.blend`；机械干涉未在本轮重新验证。

板名 `PCB2` / `PCB3_1` 是 EDA 文档名，并不代表层数。当前 PCB3_1 归档使用六层；不能根据早期“四层”方案直接下单。

**建议导入的完整原生归档**：[PCB3_1_routing_complete.epro2](PCB/backups/PCB3_1_routing_complete.epro2)。本次只上传这一份已复核工程归档，不上传重复工作数据库或历史工程。

## 目录与文件说明

| 路径 | 内容与维护方式 |
| --- | --- |
| `3D/*.SLDPRT` | SolidWorks 零件：型材、摆臂、铰链、轮毂、紧固件等；装配依赖文件名，禁止随意重命名或搬移 |
| `3D/*.SLDASM` | SolidWorks 总装与子装配；顶层为 `Vehicle Assembled.SLDASM` |
| `3D/*.STEP`、`IMG_*.jpg` | 中性几何导出与实物照片，用于尺寸参考和安装记录 |
| `3D/Vehicle Layout v15.blend` | 当前 Blender 布局场景；旧版场景不上传 |
| `tools/step_to_obj.py`、`tools/README.md` | STEP→OBJ 转换及机械资料使用说明 |
| `PCB/README.md`、`PCB/docs/` | 最终硬件交付与 PCB3_1 布线完成复核 |
| `PCB/design/AB_INTERCONNECT_v1.json` | A/B 60 位信号排线与 8 位电源线的接口契约 |
| `PCB/bom/BOM_PCB2_final.csv`、`BOM_PCB3_1_final.csv` | 从本次最终原生归档逐器件提取的 BOM，分别 139/234 个元件；不是旧工作 BOM |
| `PCB/bom/PCB3_1_填孔盖帽孔位表_v1.csv` | 最终 PCB3_1 制造工艺孔位清单 |
| `PCB/reports/` | 只保留最终截图、审计汇总与最终几何/连通性/覆铜检查结果 |
| `PCB/backups/PCB3_1_routing_complete.epro2` | 唯一发布的完整原生 EDA 归档，包含本轮核对的 A/B 板 |
| `Code/MecanumB_Control/` | 当前 STM32 编译工程，详见下表 |
| `Code/ros2_ws/src/` | 主用 MAVLink 节点，以及测试仍引用的 legacy 兼容实现；后者不作为默认部署版本 |
| `Code/ESP32_Radio_Bridge/` | 当前 ESP32 源码、PlatformIO 配置和使用说明 |
| `Code/pi/` | 当前 Pi 服务、GPIO 权限、UART/关机 overlay 模板；HALTED 模板仍待实测 |
| `Code/tools/` | 环境、时钟、硬件接口与最终 ELF 检查脚本 |
| `Code/tests/` | 当前 80 项 C 核心、ROS/PTY、GPIO、存储和健康测试及 C 测试夹具 |
| `Code/reports/` | 仅最终验证汇总与两份接口/镜像机器检查结果，不上传过程日志 |
| `Code/docs/` | 更新后的 HTML 全景报告、一致性复核、引脚定义和参数/保护说明 |
| `Code/cubemx/B板引脚分配_v1.csv` | 检查工具依赖的引脚契约；旧 CubeMX 工程本体不上传 |

本地的旧 `Code/Test1/`、`Code/cubemx/` 工程、`doc/` 大量资料、EDA 创建/候选路由脚本、旧 Blender 场景和阶段备份均未纳入本次发布。完整发布清单见 [FILES.md](FILES.md)。

### STM32 代码地图

`Code/MecanumB_Control/Makefile` 是当前构建入口，`MecanumB_Control.ioc` 为配套 CubeMX 配置。

| 文件/目录 | 职责 |
| --- | --- |
| `Src/main.c`、`Src/stm32f4xx_hal_msp.c` | 时钟、GPIO、外设、DMA 和启动顺序；用户逻辑放在受保护代码区或 App 中 |
| `Src/stm32f4xx_it.c`、`Src/freertos.c` | 中断与 FreeRTOS 配置适配 |
| `Inc/main.h`、`Inc/FreeRTOSConfig.h` | GPIO 名称、RTOS 优先级、静态分配及运行统计配置 |
| `App/src/vehicle_app.c` | HAL/RTOS 编排，Safety/Control/HostRx/Telemetry/Power 五任务、Flash 适配与统计 |
| `vehicle_control.c` | 麦轮运动学、编码器回绕差分、轮速滤波、PI、加减速与换向限制 |
| `vehicle_link.c` | 控制来源、解锁、租约、指令有效期、故障和维护联锁 |
| `vehicle_mavlink.c` | 双 UART MAVLink、20 参数、遥测、关机命令与可选本地 SBUS 控制 |
| `vehicle_protocol.c`、`vehicle_tx.c` | legacy 帧格式/CRC 和有限容量发送队列，保留用于兼容测试 |
| `vehicle_sensor_app.c` | SensorRx/Imu 两任务、UART DMA、SPI IMU、ADC 数据交接 |
| `vehicle_sensors.c`、`vehicle_fusion.c` | UBX/LD06/SBUS 解析、传感器状态与姿态估计 |
| `vehicle_power.c` | Pi 关机请求、停轮、HALTED 确认和一次性切电状态机 |
| `vehicle_health.c` | 电流、电压、可选 NTC、SOC 估算与持续计时保护 |
| `vehicle_store.c` | 两扇区参数追加日志、CRC、损坏槽跳过与启动轮换 |
| `App/include/*.h` | 上述模块的公共接口、结构体和编译期配置 |
| `STM32F429ZGTX_FLASH.ld` | 程序 Flash 768 KiB；另外两个 128 KiB 扇区存参数，禁止覆盖 |
| `Startup/`、`Drivers/`、`Middlewares/` | 启动汇编、CMSIS/HAL、FreeRTOS 10.3.1 与 MAVLink；保留上游许可证 |

表中未写完整目录前缀的 C 文件均位于 `Code/MecanumB_Control/App/src/`。

## 构建与验证

### STM32

需要 GNU Arm Embedded 工具链、GNU Make，以及用于原生 C 测试的 GCC。克隆后直接构建已提交的源代码，不必先运行 CubeMX：

```bash
make -C Code/MecanumB_Control -B -j4
python3 Code/tools/check_clock_config.py
python3 Code/tools/check_hardware_contract.py
python3 Code/tools/check_firmware_image.py
```

输出在 `Code/MecanumB_Control/build/`：`.elf` 用于调试，`.hex` / `.bin` 用于烧录，`.map` 用于分析地址和占用；这些编译产物不提交。**烧录应保留参数扇区，整片擦除会删除标定参数。**

本轮链接结果：FLASH **78,844 B / 768 KiB**，RAM **54,712 B / 192 KiB**，CCM **0 B / 64 KiB**。RAM 数值使用链接器统计，包含链接脚本预留区域，不能等同于运行时栈实际用量。运行统计的采集通路已具备，尚无设备实测数值。

### Python、C 核心与 ROS 2 测试

完整测试需要 ROS 2 Humble、`rclpy`、消息包、`pyserial`、`pymavlink` 和 GCC。当前机器已具备的环境命令如下；其他机器需先建立可导入 ROS 系统包的 Python 环境：

```bash
source /opt/ros/humble/setup.bash
~/.venvs/mecanum-ros2/bin/python -m unittest discover -s Code/tests -v
# 也可在依赖齐全的系统 Python 环境运行同一条 unittest 命令
```

结果为 **80 tests / OK / 0 skipped**；未安装 ROS 或 pymavlink 时，部分测试会跳过，不能把这种结果当作完整回归。精简后的验证证据位于 `Code/reports/verification_final_v1.json`；完整过程日志仅在开发机保留。

### ROS 2 与 Pi

```bash
source /opt/ros/humble/setup.bash
cd Code/ros2_ws
colcon build --symlink-install --packages-select mecanum_mavlink mecanum_bridge
source install/setup.bash
ros2 run mecanum_mavlink bridge --ros-args --params-file src/mecanum_mavlink/config/vehicle.yaml
```

配置中的轮径、轮距、编码器计数和方向必须实测；模板零值不能直接用于运动。`ready_gpiochip` 必须对应目标 Pi 上的 BCM GPIO17 控制器；PC 调试可以关闭 GPIO，实车不能绕过 READY 联锁。Pi 具体部署步骤见 [Code/pi/README.md](Code/pi/README.md)。

- `/cmd_vel`：运动指令输入；`/mecanum_mavlink/arm`：显式解锁/上锁。
- `/mecanum_mavlink/shutdown_pi`：先上锁再请求有序关机；成功 ACK 仅表示请求已接受。
- `/diagnostics`：链路、电气、运行统计等；无效遥测不能解释为测量值 0。
- `Code/pi/mecanum-bridge.service` 启动 `mecanum_mavlink`，默认 921600 8N1。不要同时运行旧 `mecanum_bridge`。
- `config-halted-pending.txt` 仍是待实板验证的内核 HALTED 模板；未启用时不能宣称已验证自动断电闭环。

### ESP32

```bash
~/.venvs/mecanum-esp32/bin/pio run -d Code/ESP32_Radio_Bridge
# 或使用已安装的 PlatformIO CLI：pio run -d Code/ESP32_Radio_Bridge
```

平台固定为 `espressif32@6.9.0`，UART RX/TX 为 GPIO16/17，SBUS RX 为 GPIO4，UDP 14550。修改 Wi-Fi 参数和目标环境后再烧录；禁止把个人 Wi-Fi 密码或令牌写进提交。

### 机械与 EDA

SolidWorks 2016 为机械源工具。STEP 转换需要 `cadquery-ocp`，使用方法见 `tools/README.md`；最终 Blender 场景直接打开。旧布局生成过程未上传，不能用 STEP 转换器直接重现 v15 的板卡摆放。本轮只修改固件与文档，没有更改 CAD 几何或电路布局。

EDA 使用本机 Bridge（不是 MCP HTTP 服务本身）：

```bash
python3 ~/.agents/skills/easyeda-api/scripts/bridge-client.py health
# 多窗口必须显式选择 --window；先读后写，避免重放旧布局/走线脚本
```

PCB3_1 的最终几何审计不等于嘉立创EDA原生 DRC 已跑通。加工前核对最终层叠、Gerber、钻孔、热设计和 [填孔盖帽孔位表](PCB/bom/PCB3_1_填孔盖帽孔位表_v1.csv)；本版涉及焊盘内过孔，需匹配树脂填孔、磨平及盖帽工艺。

## 保护参数与尚待完成的验证

A 板电流前端为 50 mΩ × 10 V/V，即名义 0.5 V/A；120 kΩ / 10 kΩ 电压分压比为 13。REF 约 1.5 V，对应硬件限流约 3 A。软件按用户提供的 MG370 单电机约 1 A 堵转电流设置待标定阈值：**1.2 A / 20 ms 过流、0.8 A + 低速 / 500 ms 堵转**。这些是软件配置，不能替代实际电机、采样相位和热测试。

NTC 电路为 10 kΩ 上拉、100 Ω 串联、100 nF；外接探头型号和 Beta 尚未确认，因此默认 `VH_NTC_ENABLED=0`，**现版默认没有启用温度保护**。本地 SBUS 控制默认 `VM_SBUS_AUTHORITY=0`，仅上报通道；确认通道和 failsafe 后才启用。SOC 是假定 4S 的电压估算，没有 BMS、单体监测、均衡或库仑计量。

必须补齐：实板接线与极性、SWD 烧录记录、电流/电压标定、PWM/ADC 相位、编码器方向和计数、IMU 安装姿态、外部看门狗与 Flash 轮换/掉电试验、READY/HALTED 波形、1 kHz 抖动/CPU/栈实测，以及故障注入和 HIL。每项记录测试条件、固件版本、数据和结论。

## 版本、贡献与文件管理

- 修改真实编译工程 `Code/MecanumB_Control/`；不要拿历史 `cubemx/` 或 `Test1/` 覆盖它。
- 提交前运行与改动相关的测试；描述变更原因、硬件影响、验证结果及未验证项。
- 不修改已有 `AGENTS.md`；不随意重命名机械件；EDA 写脚本先读取当前文档并备份。
- `.gitignore` 排除本机账号目录、无关 SQLite 缓存、自动滚动备份、Python/ROS/PlatformIO/STM32 编译缓存；只保留一份最终 EDA 归档、一个当前 Blender 场景、当前源码及必要最终检查证据，历史备份不上传。
- 项目未统一声明整库开源许可证；第三方 HAL、CMSIS、FreeRTOS、MAVLink 等遵循各自文件中的授权。公开可读不表示所有 CAD、资料和代码可按同一许可证再分发。
