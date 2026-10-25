# MecanumB ESP32 radio bridge

ESP32 无线桥：在 STM32 的 USART1 与 WiFi/UDP 地面站之间转发标准 MAVLink v2 帧，并把 SBUS 遥控输入转换为 `RC_CHANNELS` 同时送给 STM32 与地面站。

```text
SBUS 接收机 ── GPIO4 ─┐
                      ├─ ESP32 ── WiFi/UDP 14550 ── QGC / 地面站
STM32 USART1 ── GPIO16/17 ─┘
```

桥不参与解锁：`COMMAND_LONG(MAV_CMD_COMPONENT_ARM_DISARM)` 只转发，不主动发送。遥控输入也仅在 STM32 的仲裁逻辑允许时才可能获得运动权。

## 接线

| ESP32 | 方向 | 对端 | 说明 |
| --- | --- | --- | --- |
| GPIO16 (U2RX) | 输入 | STM32 PA9 / USART1_TX | 921600 8N1 |
| GPIO17 (U2TX) | 输出 | STM32 PA10 / USART1_RX | 921600 8N1 |
| GPIO4 | 输入 | SBUS 接收机信号 | 100000 8E2，硬件反相（`SERIAL_8E2` + invert） |
| GPIO2 | 输出 | 板载/外接 LED | 常亮 = STM32 链路正常；5 Hz 闪 = 失联 |

电平为 3.3 V。SBUS 若使用非反相接收机，需要外部反相器或改为普通 `SERIAL_8E2` 并在代码中去掉 invert 参数。

## 构建与烧录

```bash
~/.venvs/mecanum-esp32/bin/pio run  -d Code/ESP32_Radio_Bridge
~/.venvs/mecanum-esp32/bin/pio run  -d Code/ESP32_Radio_Bridge -t upload
~/.venvs/mecanum-esp32/bin/pio device monitor -d Code/ESP32_Radio_Bridge -b 115200
```

工程直接引用 `Code/MecanumB_Control/Middlewares/Third_Party/MAVLink` 中的同一份方言头文件，无远程库依赖，编译不需要联网。当前构建占用 RAM 约 14.2%、Flash 约 57.0%。

### 编译选项（`platformio.ini`）

| 宏 | 默认 | 说明 |
| --- | --- | --- |
| `BRIDGE_UART_RX/TX` | 16/17 | STM32 串口引脚 |
| `BRIDGE_UART_BAUD` | 921600 | 与固件 USART1 一致 |
| `BRIDGE_SBUS_ENABLED` | 1 | 置 0 可关闭 SBUS 解析与 `RC_CHANNELS` 发送 |
| `BRIDGE_SBUS_RX` | 4 | SBUS 输入引脚 |
| `BRIDGE_LED` | 2 | 链路指示灯 |
| `BRIDGE_UDP_PORT` | 14550 | 地面站端口 |
| `BRIDGE_SYSID/COMPID` | 2/68 | `MAV_COMP_ID_TELEMETRY_RADIO` |
| `BRIDGE_WIFI_SSID/PASS` | 空 | 非空时作为 STA 加入指定热点 |
| `BRIDGE_AP_SSID/PASS` | `MecanumB-Bridge` / `mecanum123` | SSID 为空时启动的兜底 AP |

## 网络模式

- **STA**：编译时给出 `BRIDGE_WIFI_SSID`/`BRIDGE_WIFI_PASS`，ESP32 加入现有热点（例如树莓派热点），主机名 `mecanum-bridge`，断线每 5 s 重连。
- **AP**：SSID 留空即启动 `MecanumB-Bridge` 热点，ESP32 地址 192.168.4.1。适合首次调试或无路由器场景；AP 模式无互联网。

地面站连接 UDP 14550 后即可收到遥测。ESP32 在 1 Hz 心跳中还会向 `255.255.255.255:14550` 广播，便于 QGC 自动发现；其余消息只单播给最近 15 s 内有收包的 4 个对端（超过 4 个时替换最久未活动的）。

QGC 手动添加链路：`UDP`，端口 `14550`，目标地址为 ESP32 的 IP（AP 模式为 `192.168.4.1`）。

## 链路与健康检查

| 周期 | 内容 |
| --- | --- |
| 1 Hz | ESP32 `HEARTBEAT`，写入 UART 并单播/广播；`MAV_TYPE_GENERIC`、`MAV_AUTOPILOT_INVALID` |
| 1 Hz | STM32 存活时发送 `PING` 并统计往返时延 |
| 20 ms | 仅在 SBUS 新鲜（<500 ms）且无 failsafe/lost 时发送 `RC_CHANNELS` |
| 5 s | STM32 心跳超时后发 `STATUSTEXT: STM32 MAVLink link lost` |

串口监视器每 2 s 打印 `fc=up/down gcs=up/- rtt=… sbus=up/- wifi=sta/ap ip=…`。ESP32 侧以 3 s 无 STM32 心跳判定链路丢失；STM32 侧对 ESP32 心跳有 2 s 期限（`VL_MAV_LINK_TIMEOUT_US`）、对遥控有 250 ms 期限（`VM_RC_TTL_MS`）；SBUS 失效或超时后桥停止发送 `RC_CHANNELS`，让固件的 250 ms 期限自然触发遥控失效保护。ESP32 不影响 Pi 链路的授权判定。

## 遥控通道

`RC_CHANNELS` 按 SBUS 顺序原样发送 16 路，SBUS 的 172–1811 映射到 MAVLink 的 1000–2000（中位约 1500）。固件的运动映射为 CH1 横移、CH2 前进、CH4 偏航，CH3/CH5 及以上当前未参与运动控制。首次使用前应断开电机、观察 `ros2 topic echo /rc/channels` 确认通道顺序与方向。
