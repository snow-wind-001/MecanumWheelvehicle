> 2026-10-25 部署基线：`mecanum-bridge.service` 现在启动 **mecanum_mavlink**，与 STM32 默认 MAVLink 镜像一致。复制配置时使用 `Code/ros2_ws/src/mecanum_mavlink/config/vehicle.yaml`；旧 `mecanum_bridge` 仅用于明确切换到 legacy 固件的实验。两个节点不得同时占用串口或 GPIO17。

# Raspberry Pi 4 部署与 GPIO 握手

适用于 Pi 4、ROS 2 Humble。以下是待装机核验的部署文件，不会修改本机系统。当前 MCU 已执行 Pi READY/PGOOD/HALTED 联锁，A 板保持电路已加入原理图/PCB；MCU 关机请求和切电脉冲状态机已接入，但实板时序尚未验证。

## UART 与正常关机请求

1. 在 Pi 镜像实际使用的 `config.txt` 合并 `config-uart-shutdown.txt`（常见位置 `/boot/firmware/config.txt` 或 `/boot/config.txt`）。移除实际启动命令行中的 `console=serial0,...`/`console=ttyAMA0,...`，保留其他参数。
2. 禁止 `hciuart.service` 占用 UART0；重启后用 `readlink -f /dev/serial0` 核对 Pi 4 的 PL011 映射，并确认串口控制台未启用。双方 921600 8N1；仍需示波器/实串口测误码。
3. GPIO27 为低有效请求，交给 `gpio-shutdown` 和 systemd-logind；`60-mecanum-power.conf` 配置专用车载系统的电源键行为。不要由 ROS 把“请求已收到”当作内核停机完成。

## ROS READY 与权限

安装 `python3-libgpiod`、`gpiod`、`python3-serial`；ROS Python 环境需能导入系统包。驱动支持 libgpiod 1.x 和 2.x，PC 默认不启用 GPIO；`../ros2_ws/src/mecanum_mavlink/config/vehicle.yaml` 为 Pi 4 配置 `/dev/gpiochip0`。用 `gpioinfo` 识别 BCM 控制器与 GPIO17，不能把 Pi 5 的编号直接套用。

ROS 节点申请 GPIO17 时先输出低，只有 MAVLink 心跳新鲜（默认 2 s）才置高；失联、会话丢失或正常退出拉低。硬退出时依赖线路释放后的外部下拉，串口命令期限仍独立生效。无法取得配置的 GPIO 会使节点启动失败，不能默认为已就绪。READY 不代表已解锁，MCU 还要求供电有效、HALTED=低、显式授权及新鲜运动指令。

`mecanum-bridge.service` 约定专用用户 `mecanum`、安装前缀 `/opt/mecanum/ros2_ws` 和配置 `/etc/mecanum/vehicle.yaml`。在目标 Pi 建立用户及 `gpio` 组，赋予 `dialout`/`gpio` 权限，安装 udev 规则后重新加载规则。用该用户先前台运行，再安装/启用服务。车辆几何、编码器计数和方向参数须实测；配置中的零值不能解锁。网络中的 ROS_DOMAIN_ID 应保持一致。

## 内核 HALTED 与待完成项

`config-halted-pending.txt` **暂不合并**。官方 gpio-poweroff 要求外部电源控制器在信号激活后切电；MCU 已实现根据 HALTED 发 PD3 清除 A 板保持锁存，尚无真实接线和波形证据。启用前必须在实板验证该流程，否则内核会等待断电并可能告警。

GPIO22 只能由内核 gpio-poweroff 使用：默认低，关机末期先高 100ms、低 100ms、再高；初次有效高后即可切电。Python、ROS、systemd 的停止回调均不得伪造 HALTED。验证还须覆盖正常关机、重启、MCU 复位、Pi 掉电及线束断开。

资料：[Raspberry Pi 官方 overlays 说明](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README)、[libgpiod 官方 Python 示例](https://github.com/brgl/libgpiod/blob/master/bindings/python/examples/toggle_line_value.py)。

有序关机服务为 `/mecanum_mavlink/shutdown_pi`（Trigger），须先停车并撤销授权；成功只表示已接受请求。静态 PowerTask 及状态、故障诊断见 [Pi有序关机与切电](../docs/代码与电路一致性复核_v1.md)。内核等待参数为 10s，装机时仍须验证停轮和切电余量。
