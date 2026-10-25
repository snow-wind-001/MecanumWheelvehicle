#!/usr/bin/env python3
"""Render the final offline FreeRTOS engineering report from checked evidence.

Requires the Python Markdown package. No build, EDA write or hardware action.
Example: python3 Code/tools/render_freertos_report.py
"""
from html import escape
import json
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / 'Code/docs'


def main():
    evidence = json.loads((ROOT / 'Code/reports/verification_final_v1.json').read_text())
    contract = evidence['hardware_contract']
    pins = '\n'.join('| {role} | {port} | {package_pin} | {archived_net} | {status} |'.format(
        **{k: (str(v) if v != '' else '未接') for k, v in row.items()}) for row in contract['rows'])
    audit = (DOCS / '代码与电路一致性复核_v1.md').read_text().split('\n', 1)[1]
    audit = '\n'.join('#' + line if line.startswith('##') else line for line in audit.splitlines())
    body = r'''
## 阅读结论与证据等级 {#overview}

当前交付具备软件台架联调条件：**80 项测试通过、零跳过，STM32 全量构建零告警，ESP32 / ROS 构建成功，必需引脚与板间接口匹配**。这份报告同时记录已修复的问题与未验证项。

未完成真实烧录、ADC/PWM 标定、HIL、机械干涉复核与运行统计实测。NTC 保护和本机 SBUS 运动权默认关闭。**不得把“软件测试通过”解释为整车或工业认证已经完成。**

| 证据 | 能说明什么 | 不能说明什么 |
| --- | --- | --- |
| C/Python/ROS/PTY 80 项测试 | 在主机上验证真实协议/状态/存储/保护核心及串口交互 | 不验证真实 HAL 外设、模拟噪声或电机负载 |
| STM32 / ESP32 / ROS 构建 | 当前源码可编译、链接与打包 | 不证明固件已烧入板卡 |
| 原生归档解析与 GPIO 对照 | 64 个必需引脚、60 位信号与 8 位电源接口契约一致 | 不证明实体线束针序、极性和焊接良好 |
| SRAM 函数反汇编检查 | 擦除轮询不依赖 Flash 函数调用，且程序未侵入参数区 | 不证明擦除过程的 WDI 波形与实际最长时间 |
| 既有 PCB3_1 最终几何审计 | 保存后铜形间距与信号/GND 连通性复核通过 | 不能替代未跑通的嘉立创EDA原生 DRC 或生产文件审查 |

## 固件启动：先建立禁止输出，再建立任务 {#boot}

1. 复位向量经启动汇编复制 `.data`（包含 `.RamFunc`）到 SRAM、清零 `.bss`，进入 HAL 与时钟初始化。
2. HSE=8 MHz，PLL M/N/P=8/360/2，SYSCLK=180 MHz，APB1=45 MHz，APB2=90 MHz。外设初始化由 `Src/main.c` 与 MSP 完成。
3. GPIO、DMA、TIM、ADC、SPI 与 UART 初始化。电机许可及主输出保持关闭；外部急停/禁止线不能由软件绕过。
4. `vehicle_app_init()` 启用 DWT、生成 boot_id、初始化授权状态并扫描 Flash A/B 日志。有有效参数才设置 configured；加载不会自动解锁。
5. 必须擦除时只在启动阶段执行 SRAM 擦除/看门狗服务，绝不在运行任务中擦除扇区；之后创建七个静态任务。
6. 再启动内部 IWDG，进入调度器。SafetyTask 只有在所监测任务持续推进时才维持正常看门狗服务。

启动阶段和正常运行阶段的喂狗策略不同。RAM 维护函数仅用于**电机被禁止的启动擦除**，3 s 超时会停止喂狗；不能把它移入运动任务作为无限续命机制。

## 七个任务与中断边界 {#tasks}

| 任务 | 优先级 | 静态栈（32 位字） | 名义节拍 / 触发 | 职责 |
| --- | --- | --- | --- | --- |
| Safety | 15 | 512 | 2 ms | 联锁、急停/故障、授权期限和健康进度；看门狗 |
| Control | 14 | 1024 | TIM6，1 kHz | 编码器、PI、输出、健康保护、抖动统计 |
| HostRx | 11 | 1536 | 1 ms 轮询 / DMA 接收 | Pi/ESP32 双通道解帧、命令与参数 |
| Imu | 10 | 768 | DRDY / 有限等待 | SPI 采样、时间戳、姿态输入 |
| SensorRx | 8 | 1024 | 周期处理循环 DMA | UBX、LD06、SBUS 解析、ADC 交接 |
| Telemetry | 7 | 1024 | 1 ms 调度 | 有限队列遥测、配置防抖持久化、CPU 采样 |
| Power | 6 | 512 | 10 ms | Pi 停轮、关机握手、HALTED 与切电状态机 |

栈值为分配容量，不是测量出的余量。高水位遥测单位同为字，乘 4 才是字节。ISR 处理时间戳、DMA 进度、通知与紧急关闭；高紧急度中断不得直接调用不满足 FreeRTOS 优先级约束的 API。协议、健康和电源核心保持纯 C，应用层负责临界区串行化。

## 1 kHz 控制路径与运动安全 {#control}

```text
TIM6 → 通知 Control → 时间差/期限检查 → 读 TIM2/3/4/5 编码器
     → 配置版本同步 → 检查运动许可 → 麦轮逆解/限速/斜率限制
     → 编码器差分/轮速滤波 → PI + 前馈/饱和与换向限制
     → 电气健康检查 → 再检查硬件联锁 → DIR / CCR / MOE / ARM
```

轮序固定 FL、FR、RL、RR。编码器 16 位计数器按有符号回绕差分处理，32 位计数器使用相应模数；CPR 必须是减速输出轴每圈经正交解码后的有效计数，不能直接填传感器单路脉冲数。

几何参数包括轮半径、半轴距、半轮距；方向符号和轮安装顺序必须通过单轮测试确认。轮速环不是电流环，采样电流用于诊断与保护，不能据此宣称具备已标定的力矩控制。

运动许可由已授权来源、租约、指令有效期、无故障、无电源禁止、无 Flash 维护组成；Pi 额外要求 READY。紧急关闭同时撤 ARM、关闭 TIM1 MOE 并清占空比。失联或故障后恢复数据不自动恢复运动。

正常稳态周期超预算产生控制故障；启动 100 ms 和禁止运动的 Flash 维护窗口不计入正常抖动样本。窗口存在必须在实测报告中注明，不能用被排除的样本证明整车实时性。

## 控制链路、参数和坐标 {#protocol}

| 通道/入口 | 配置 | 主要消息/服务 | 约束 |
| --- | --- | --- | --- |
| STM32 USART6 ↔ Pi | 921600 8N1；MCU sysid/compid 1/1 | HEARTBEAT、COMMAND_LONG、PARAM_*、SET_POSITION_TARGET_LOCAL_NED | READY、显式 ARM、心跳和运动指令都必须有效 |
| STM32 USART1 ↔ ESP32 | 921600 8N1 | MAVLink 遥测、RC/无线转发 | 不能绕过 MCU 授权/故障期限 |
| ROS `/cmd_vel` | geometry_msgs/Twist | 车体运动请求 | 超时必须停车；节点不自动重解锁 |
| `~/arm` | SetBool | ARM / DISARM | configured、无故障且符合联锁 |
| `~/shutdown_pi` | Trigger | 命令 246，param1=0，param2=2 | 仅 Pi、先上锁，ACK 后等待真实内核停机 |

MAVLink 车体消息使用 BODY_NED/FRD 约定，ROS 采用常见 x 前、y 左、z 上；节点承担横向/偏航符号转换。IMU 轴映射是编译期默认，必须按板卡实际朝向校准。姿态使用 IMU 滤波与轮速辅助，不是 EKF、多传感器全局定位或视觉/激光 SLAM。

20 个配置参数：`RADIUS_M`、`HALF_LEN_M`、`HALF_WID_M`、`COUNTS_REV`、`ENC_SIGN0..3`、`MOT_SIGN0..3`、`MAX_WHEEL`、`MAX_ACCEL`、`KP`、`KI`、`FF`、`DUTY_LIMIT`、`SPEED_TAU`、`REVERSAL`。仅 DISARMED 接受参数修改；有效配置防抖后落盘。模板中的几何零值用于阻止未标定运动，不能直接改成看似合理的猜测值。

## 传感器链路与真实性 {#sensors}

| 路径 | 实现 | 新鲜度/故障处理 | 尚待验证 |
| --- | --- | --- | --- |
| ICM-42688-P | SPI2 + PD2 DRDY；短事务不抢占 SBUS DMA 流 | 记录边沿和采样时间，有限等待 | 型号响应、轴向、噪声、200 Hz 实际节拍 |
| GNSS | USART2 DMA，UBX 解析，PG8 PPS | 校验与重同步，数据时间戳 | 天线、固件协议、锁定与定位精度 |
| LD06 | USART3 RX DMA，CRC/帧同步 | 局部供电、支路故障与超时状态 | 线序、转速模式、实测角度/距离 |
| SBUS | UART7 PF6，100000、偶校验、2 stop；硬件完成电平/反相 | SBUS 无 CRC；格式/failsafe/新物理帧时间 | 接收机极性、CH5、断链输出 |
| ADC | PC0–PC5/PB1，TIM8 TRGO + DMA | ADC 有效性与 1 s 丢失故障 | 参考电压、零点、PWM 有效采样窗口 |

本机 SBUS 运动权默认关闭。启用分支已专门编译并测试：先低后高、一次消费上升沿、缓存帧不续租、failsafe 立即撤权。它没有替用户确认接收机的真实 failsafe 行为。

## Pi 电源流程与部署闭环 {#power}

```text
显式关机请求 → 锁存 power_inhibit → 确认驱动关闭且停轮
→ PC6 请求（A 板转换为 GPIO27 低）→ Linux 有序关机
→ 内核 GPIO22 HALTED → MCU PD1 确认 → PD3 单次切电脉冲
```

PowerTask 用实际输出施加时间更新状态机，避免抢占缩短物理脉冲。停止、HALTED 或断电超时只报故障并保留供电，不强行切电，也不自动重复脉冲。重复已接受关机命令可以 ACK，但不重新启动状态机。

`Code/pi/mecanum-bridge.service` 当前启动 **mecanum_mavlink**。配置必须来自同名包，不能使用 legacy 包的参数 namespace。节点启动先将 GPIO17 READY 置低，只有心跳新鲜才拉高；成功关机 ACK 后保持低，退出时释放。两个 ROS 节点不可同时占用串口或 READY。

GPIO22 必须归内核 `gpio-poweroff` 所有。发布的 `config-halted-pending.txt` 是等待真实接线/波形验证的模板，不能把模板存在当作已启用。ROS 停止回调或 Python 不得模拟 HALTED。

## 遥测、诊断与统计单位 {#telemetry}

| 字段组 | 字段示例 | 单位/无效值 |
| --- | --- | --- |
| 电气 | i_fl/i_fr/i_rl/i_rr、ntc_c、soc、vbat、vmot | A、°C、0–1、V；无效模拟量为 NaN |
| 保护 | fault、stall | 故障位图与四轮堵转位图，不能当连续量画趋势 |
| 参数 | cfg_rev、cfg_seq、cfg_err | RAM 版本、成功存储序号与错误计数 |
| 任务栈 | stk_safe/stk_ctrl/stk_host/stk_tele/stk_pow/stk_imu/stk_rx | 最低剩余栈字数；不是已用字节数 |
| 实时性 | jit_us、jit_max、cpu_pct | 绝对周期误差 µs、最大值 µs、估算 CPU % |
| 通信 | pi_up、esp_up、esp_age、rc_age | 活跃标志与秒龄；无历史数据不可解释为“刚收到” |

ROS `/diagnostics` 汇总 serial link、electrical、runtime 等状态。CPU 是 DWT 与 Idle 计数估算，不是独立仪器测量；微秒计数接口约 71 分钟回绕，上游 FreeRTOS 统计在边界处有精度限制。故障位、配置序号以 MAVLink 浮点命名字段发送时，大整数还受浮点精度限制，不能依赖其进行无限精确的长期审计。

## 本轮缺陷修复、硬件依据与验证 {#review}

''' + audit + r'''

## 完整 MCU 信号对照 {#pins}

下表由最终 EDA 原生归档自动生成。`pass` 表示归档 MCU 网络与引脚契约一致；对具有主 GPIO 宏的项目还检查端口。它不穷尽外设寄存器配置、封装焊接或实物线束测试。

<div class="pin-search"><label for="pin-query">筛选引脚 / 网络：</label><input id="pin-query" type="search" placeholder="例如 PG12、NTC、PI_READY" autocomplete="off"></div>
<div id="pin-table" markdown="1">

| 功能 | MCU 端口 | LQFP144 脚号 | 归档铜网络 | 结果 |
| --- | --- | --- | --- | --- |
''' + pins + r'''

</div>

## 代码文件地图与修改入口 {#files}

| 当前文件/目录 | 作用 | 修改注意 |
| --- | --- | --- |
| Src/main.c / MSP / it.c | 外设初始化、时钟、DMA 与 IRQ | 结合 IOC、物理引脚、DMA 流和 IRQ 优先级审核 |
| App/src/vehicle_app.c | RTOS/HAL 编排、输出、Flash 适配、统计 | 维护与 ARM 必须同临界区串行化；SRAM 擦除不能新增 Flash 调用 |
| vehicle_control.c | 麦轮运动学、编码器、滤波、PI | 轮序、符号、CPR 与坐标约定须保持一致 |
| vehicle_link.c | 显式授权、来源、期限、维护/电源/故障联锁 | 新协议入口必须复用同一许可判断 |
| vehicle_mavlink.c | MAVLink 解析、参数、遥测、SBUS、关机命令 | 确认来源通道、目标 ID、数值范围及回复幂等性 |
| vehicle_sensor_app.c / vehicle_sensors.c | HAL 采集与纯 C 帧解析 | 不能把缓存转发时间冒充物理采样时间 |
| vehicle_fusion.c | 姿态及轮速辅助估计 | 校准 IMU 朝向；不要误写为全局融合定位 |
| vehicle_power.c | Pi 停轮→HALTED→切电 | 真实输出施加时间、超时保电和单次脉冲 |
| vehicle_health.c | ADC 换算和持续计时保护 | 阈值、有效性和恢复条件同时校验 |
| vehicle_store.c | 固定长度参数日志 | 只追加空白位置；CRC、损坏恢复与掉电语义 |
| vehicle_protocol.c / vehicle_tx.c | legacy 兼容编码和发送队列 | 当前测试依赖这些实现；不是旧快照备份 |
| App/include/ | 上述模块公共接口、状态和配置宏 | 修改结构后重跑完整主机测试与固件构建 |
| Drivers / Middlewares / Startup | HAL、CMSIS、FreeRTOS、MAVLink 与启动汇编 | 保留原许可证，不用实验工程覆盖当前源码 |
| ROS mecanum_mavlink | 主用节点、GPIO17、服务、遥测/诊断 | 验证 PTY、命令 ACK、GPIO 释放与过期行为 |
| ROS mecanum_bridge | 测试保留的 legacy 兼容实现 | 不与主用节点同时部署 |
| ESP32_Radio_Bridge | 当前无线/串口桥 | UART 接线、SBUS 极性、网络参数均须现场确认 |
| tests/ | C 测试夹具与 unittest | 当前共 80 项；完整运行要求 ROS + pymavlink |
| tools/check_hardware_contract.py | 原生归档引脚和板间接口复核 | 更换归档时重跑 |
| tools/check_firmware_image.py | ELF SRAM 函数与程序 Flash 边界检查 | 每次改擦除函数/链接配置后重跑 |
| tools/render_freertos_report.py | 用最终 JSON 和本轮复核文档生成本报告 | 需要 Python Markdown；不接触硬件 |

未写前缀的 C 文件位于 `Code/MecanumB_Control/App/src/`。更多文件见[根 README](../../README.md)与[完整发布文件索引](../../FILES.md)。旧实验工程、生成中间文件、历史日志不在 GitHub 发布范围。

## 复现命令与验收记录 {#reproduce}

```bash
make -C Code/MecanumB_Control -B -j4
python3 Code/tools/check_clock_config.py
python3 Code/tools/check_hardware_contract.py
python3 Code/tools/check_firmware_image.py
source /opt/ros/humble/setup.bash
~/.venvs/mecanum-ros2/bin/python -m unittest discover -s Code/tests -v
~/.venvs/mecanum-esp32/bin/pio run -d Code/ESP32_Radio_Bridge
cd Code/ros2_ws
colcon build --symlink-install --packages-select mecanum_mavlink mecanum_bridge
```

不同机器应换成实际可导入 ROS 的 Python 环境；缺依赖导致跳过不能算完整测试。构建输出不上传 GitHub，最终机器结果位于 `Code/reports/verification_final_v1.json`。部署前保留参数扇区；烧录整片擦除将清空参数。

下一阶段验收记录应逐项填写固件版本、板卡版本、仪器/接线、操作、预期、实测与结论：首次限流上电 → 单轮编码器与换向 → 电流/电压标定 → ADC/PWM 相位 → 急停/断链 → 看门狗/Flash 轮换与掉电 → Pi 有序关机 → 抖动/栈/CPU 长时统计 → 温升与 HIL。当前没有实测结果的单元应保留“未测”，不能填零。
'''
    md = markdown.Markdown(extensions=['tables', 'fenced_code', 'toc', 'attr_list', 'md_in_html'], extension_configs={'toc': {'toc_depth': '2-2'}})
    content = md.convert(body)
    css = '''
:root{--bg:#eef1ef;--paper:#fff;--ink:#172a22;--muted:#52665b;--line:#ccd8d0;--green:#163f30;--amber:#946017}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.85 system-ui,"Noto Sans SC","Microsoft YaHei",sans-serif}a{color:#17694d;text-underline-offset:3px}header{background:var(--green);color:white;padding:44px max(24px,calc((100vw - 1400px)/2));}header p{max-width:920px;color:#d6e7de}h1{font-size:clamp(30px,4vw,46px);line-height:1.3;margin:12px 0}header .tag{font-size:13px;letter-spacing:.12em}header .metrics{display:flex;gap:12px;flex-wrap:wrap;margin-top:24px}.metrics div{padding:10px 22px;border:1px solid #587c69;border-radius:5px}.metrics strong{font-size:26px;display:block}.metrics span{font-size:13px;color:#d6e7de}.toolbar{display:flex;gap:18px;align-items:center;margin-top:20px}.toolbar a{color:#fff}.toolbar button{border:1px solid #adc9ba;background:transparent;color:white;padding:7px 15px;border-radius:4px;cursor:pointer}.layout{display:grid;grid-template-columns:250px minmax(0,1fr);max-width:1450px;margin:auto;gap:32px;padding:28px 24px}nav{position:sticky;top:16px;align-self:start;max-height:94vh;overflow:auto;padding:12px 0;font-size:14px}nav ul{list-style:none;padding:0}nav li{margin:0 0 9px}nav a{text-decoration:none}article{background:var(--paper);padding:24px 38px;min-width:0;border:1px solid var(--line)}h2{font-size:26px;line-height:1.5;border-bottom:2px solid var(--line);padding-bottom:12px;margin:50px 0 20px;scroll-margin-top:20px}h2:first-child{margin-top:0}h3{font-size:21px;margin-top:34px}p{margin:12px 0}table{border-collapse:collapse;display:block;width:100%;overflow-x:auto;margin:20px 0;font-size:14px;line-height:1.65}th,td{padding:10px 12px;border:1px solid var(--line);vertical-align:top;min-width:95px}th{background:#eaf1ed;text-align:left}tbody tr:nth-child(even){background:#f8faf9}code{font: .88em/1.65 ui-monospace,SFMono-Regular,Consolas,monospace;overflow-wrap:anywhere;background:#edf2ef;padding:2px 4px;border-radius:3px}pre{white-space:pre-wrap;overflow:auto;background:#152d22;color:#e4f0e9;padding:18px;line-height:1.7}pre code{background:transparent;color:inherit;padding:0}blockquote{border-left:4px solid var(--amber);margin:20px 0;padding:8px 18px;background:#fff6e7}li{margin:6px 0}.pin-search{padding:14px;background:#edf3ef}.pin-search input{font:inherit;padding:7px 10px;max-width:100%;border:1px solid #9ab3a3;border-radius:4px}footer{max-width:1400px;margin:15px auto 40px;color:var(--muted);padding:0 24px;font-size:13px}[hidden]{display:none!important}@media(max-width:950px){.layout{display:block;padding:15px}nav{position:static;max-height:260px;margin-bottom:20px;border-bottom:1px solid var(--line)}article{padding:20px}h2{font-size:23px}header{padding:30px 22px}}@media print{body,article{background:white}header{background:white;color:black;padding:0 0 15px}header p,.metrics span{color:#333}.toolbar,nav,.pin-search{display:none}.layout{display:block;padding:0}article{border:0;padding:0}h2{break-after:avoid}tr,pre{break-inside:avoid}table{display:table;font-size:10px}th,td{padding:5px}a{color:black}footer{padding:0}.metrics div{border-color:#777}header .metrics{gap:10px}pre{background:#f0f0f0;color:black}}
'''
    script = '''document.getElementById('pin-query').addEventListener('input',function(){const q=this.value.toLowerCase().trim();document.querySelectorAll('#pin-table tbody tr').forEach(r=>{r.hidden=!r.textContent.toLowerCase().includes(q);});});'''
    html = f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><link rel="icon" href="data:,"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="description" content="MecanumB FreeRTOS 最终软件与硬件一致性报告"><title>MecanumB · FreeRTOS 程序报告</title><style>{css}</style></head>
<body><header><div class="tag">STM32F429ZGT6 / FREERTOS 10.3.1 / MAVLINK V2</div><h1>FreeRTOS 程序与电路一致性报告</h1><p>从真实代码、最终 EDA 归档与本轮软件验证出发，说明启动、调度、控制、保护和 Pi 电源握手。软件检查通过；实板与整车验收待完成。</p><div class="metrics"><div><strong>80 / 80</strong><span>主机测试 · 零跳过</span></div><div><strong>64 + 68</strong><span>必需 MCU 引脚 + 板间接口位置</span></div><div><strong>7</strong><span>静态任务 · 1 kHz 控制目标</span></div><div><strong>0</strong><span>STM32 构建告警</span></div></div><div class="toolbar"><a href="../../README.md">项目 README</a><a href="代码与电路一致性复核_v1.md">复核原文</a><button type="button" onclick="window.print()">打印 / 导出 PDF</button></div></header><div class="layout"><nav aria-label="报告目录"><strong>报告目录</strong>{md.toc}</nav><article>{content}</article></div><footer>版本：2026-10-25 最终发布 · 验证时间：{escape(evidence['verified_at_utc'])} · 无外部字体或脚本依赖，可离线阅读。本文无实板实测数据。</footer><script>{script}</script></body></html>'''
    output = DOCS / 'FreeRTOS程序报告_v1.html'
    output.write_text(html)
    print(f'Rendered {output.relative_to(ROOT)}: {len(html.encode()):,} bytes')


if __name__ == '__main__':
    main()
