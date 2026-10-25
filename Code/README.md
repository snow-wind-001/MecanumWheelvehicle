# 当前固件与宿主程序

本次发布只包含当前 STM32 / ESP32 / ROS 2 源码、测试、部署模板及最终文档。旧实验工程、过程日志、编译产物与历史 CubeMX 工程保留在开发机，不上传。

2026-10-25 软件验证：**80 项测试全部通过、无跳过；STM32 全量构建零告警，ESP32 和两个 ROS 包构建成功**。归档中的 64 个必需 MCU 引脚、60 位信号和 8 位电源接口匹配。当前仍无实板烧录、标定、HIL 和抖动/负载实测，不能视为整车验收完成。

## 文档入口

- [FreeRTOS 程序报告](docs/FreeRTOS程序报告_v1.html)：离线可读、可打印；包含可点击系统结构图、可播放/单步/切换故障分支的流程演示、麦轮逆解和 PI 交互计算，以及完整引脚表和验证边界。
- [代码与电路一致性复核](docs/代码与电路一致性复核_v1.md)：本轮修复和电路依据。
- [参数持久化与健康保护](docs/参数持久化与健康保护_v1.md)：Flash、阈值、SBUS、READY 和统计。
- [B 板引脚定义](docs/B板引脚定义_v1.md)：端口、物理脚及预留未接接口。
- [最终验证结果](reports/verification_final_v1.json)：精简机器证据；不含过程日志。
- [根 README](../README.md) 与 [完整文件索引](../FILES.md)：工程导航及全部发布文件说明。

## 目录与当前入口

| 路径 | 职责 |
| --- | --- |
| `MecanumB_Control/Makefile` | 当前 STM32F429ZG 编译入口；不要用历史工程覆盖 |
| `MecanumB_Control/Src/`、`Inc/` | HAL 初始化、中断、GPIO 与 FreeRTOS 配置 |
| `MecanumB_Control/App/src/`、`include/` | 控制、授权、传感器、通信、电源、健康和参数存储核心 |
| `MecanumB_Control/Drivers/`、`Middlewares/`、`Startup/` | HAL/CMSIS、FreeRTOS 10.3.1、MAVLink 与启动汇编，保留原许可证 |
| `MecanumB_Control/MecanumB_Control.ioc` | 当前 CubeMX 配置，生成前与实际代码核对 |
| `MecanumB_Control/STM32F429ZGTX_FLASH.ld` | 768 KiB 程序 Flash，另 256 KiB 保留参数；192 KiB SRAM、64 KiB CCM |
| `ros2_ws/src/mecanum_mavlink/` | 默认 ROS 2 MAVLink 节点，详见其 README |
| `ros2_ws/src/mecanum_bridge/` | 小型 legacy 兼容实现，当前回归测试仍依赖；不是主用部署版本 |
| `ESP32_Radio_Bridge/` | 当前 PlatformIO 无线/串口桥工程 |
| `pi/` | 主用 systemd 服务、GPIO 权限、UART/关机 overlay 模板 |
| `cubemx/B板引脚分配_v1.csv` | 保留的机器可读引脚契约；不包含历史 CubeMX 工程本体 |
| `tools/` | 环境、时钟、归档接口、ELF 检查与 HTML 报告生成器 |
| `tests/` | 生产 C 核心测试夹具、Python unittest、ROS/PTY 与 GPIO 测试 |
| `docs/`、`reports/` | 最终说明与精简验证结果 |

## 构建、测试与报告

在仓库根目录执行：

```bash
make -C Code/MecanumB_Control -B -j4
python3 Code/tools/check_clock_config.py
python3 Code/tools/check_hardware_contract.py
python3 Code/tools/check_firmware_image.py
source /opt/ros/humble/setup.bash
~/.venvs/mecanum-ros2/bin/python -m unittest discover -s Code/tests -v
~/.venvs/mecanum-esp32/bin/pio run -d Code/ESP32_Radio_Bridge
```

完整回归需要 GCC、ROS 2 Humble 的 Python 依赖、pyserial 和 pymavlink。示例虚拟环境是开发机路径，在其他机器应替换为实际可导入 ROS 的 Python；缺依赖导致跳过不等于完整回归。

STM32 产物位于 `MecanumB_Control/build/`：ELF、BIN、HEX、MAP，不提交。当前链接报告为 FLASH 78,844 B、RAM 54,712 B、CCM 0；RAM 使用链接器统计，不等同于任务实测栈占用。烧录保留 0x080C0000/0x080E0000 参数扇区，整片擦除会丢失标定。

ROS 构建：

```bash
source /opt/ros/humble/setup.bash
cd Code/ros2_ws
colcon build --symlink-install --packages-select mecanum_mavlink mecanum_bridge
source install/setup.bash
ros2 run mecanum_mavlink bridge --ros-args --params-file src/mecanum_mavlink/config/vehicle.yaml
```

报告可由最终 JSON、复核文档及 `tools/report_assets/` 中的 HTML/CSS/JavaScript 资源重新生成，需要 Python `Markdown` 包。生成器把交互资源全部内嵌到最终 HTML，复制单个报告也可离线运行；演示不连接设备、不代表实测数据：

```bash
python3 Code/tools/render_freertos_report.py
```

## 协议、部署与保护配置

MCU 默认 MAVLink v2，sysid/compid 1/1；Pi USART6 与 ESP32 USART1 均为 921600 8N1。Pi 部署服务 `mecanum-bridge.service` 实际启动 `mecanum_mavlink`，YAML 必须来自同名包。两个节点不得同时使用串口或 GPIO17。

有效参数从 A/B Flash 日志恢复或首次由主机下发，加载不自动解锁。运动必须显式 ARM、有新鲜指令、有效租约且无故障/维护/电源禁止；Pi 还要求真实 READY。`~/shutdown_pi` 仅接受上锁后的有序关机，ACK 不代表已经切电。GPIO22 HALTED 必须来自内核；待验证 overlay 不默认视为可用。

MG370 约 1 A 堵转电流对应本轮待标定阈值：1.2 A/20 ms 过流，0.8 A+低速/500 ms 堵转。NTC 型号/Beta 未确认，`VH_NTC_ENABLED=0`；本机 SBUS 映射未实测，`VM_SBUS_AUTHORITY=0`，默认只上报通道。SOC 为假定 4S 的电压估算，现版无 BMS。

TIM8 ADC 与 TIM1 PWM 同频但未硬件锁相，低侧采样的可观测窗口、零点与量程必须实测。软件状态机和测试覆盖不能替代实际波形、故障注入、温升、HIL 与 1 kHz 抖动/CPU/栈测量。
