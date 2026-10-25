# Mecanum MAVLink v2 bridge

面向 ROS 2 Humble 的 MAVLink v2 节点，连接树莓派 `/dev/serial0`（921600、8N1）与 STM32F429 USART6。节点使用标准 MAVLink 方言（`common` + `TARGET32`），不依赖自定义协议；同一套消息也供 ESP32 图传/遥控链路使用。

```text
STM32 USART6 ── MAVLink v2 ── Pi (本节点) ── ROS 2: /cmd_vel, /imu/data, /odom, /fix, /diagnostics
STM32 USART1 ── MAVLink v2 ── ESP32（遥控 + UDP 遥测）
```

本节点不直接驱动 ESP32：STM32 在 Pi 与 ESP32 之间仲裁运动权，节点只上报仲裁状态和两条链路的新鲜度。

## 构建与启动

```bash
source /opt/ros/humble/setup.bash
source ~/.venvs/mecanum-ros2/bin/activate
cd Code/ros2_ws
python -m colcon build --packages-select mecanum_mavlink --symlink-install
source install/setup.bash
ros2 launch mecanum_mavlink bridge.launch.py
```

`config/vehicle.yaml` 中的几何/计数默认值为 0，固件 `vc_config_valid()` 会拒绝该配置，因此车辆在填入实测值前无法解锁。

## 标定参数（必需）

| ROS 参数 | 固件参数 | 默认 | 说明 |
| --- | --- | --- | --- |
| `calibration.radius_m` | `RADIUS_M` | 0.0 | 轮半径，m |
| `calibration.half_length_m` | `HALF_LEN_M` | 0.0 | 半轴距，m |
| `calibration.half_width_m` | `HALF_WID_M` | 0.0 | 半轮距，m |
| `calibration.counts_per_rev` | `COUNTS_REV` | 0 | 轮端每圈计数（含倍频） |
| `calibration.encoder_sign_0..3` | `ENC_SIGN0..3` | 1 | 编码器方向符号 ±1 |
| `calibration.motor_sign_0..3` | `MOT_SIGN0..3` | 1 | 电机方向符号 ±1 |
| `calibration.max_wheel_rad_s` | `MAX_WHEEL` | 20.0 | 轮速上限 |
| `calibration.max_wheel_accel_rad_s2` | `MAX_ACCEL` | 100.0 | 轮加速度上限 |
| `calibration.wheel_kp/ki` | `KP`/`KI` | 0.1/0.2 | 速度环 PI |
| `calibration.feedforward` | `FF` | 0.02 | 速度前馈 |
| `calibration.duty_limit` | `DUTY_LIMIT` | 0.5 | 占空比限幅 |
| `calibration.speed_filter_s` | `SPEED_TAU` | 0.01 | 速度滤波时间常数 |
| `calibration.reversal_rad_s` | `REVERSAL` | 0.5 | 换向门限 |

`autoconfigure: true` 时，节点只在**未解锁**状态下通过标准 `PARAM_SET` 下发与回显不一致的参数，并以 5 s 周期发送 `PARAM_REQUEST_LIST` 校验；只有全部回显一致且取值通过固件同样的范围检查，`calibrated` 才为真。修改标定需重启固件或先解锁再上电，因为固件只允许在未解锁时写参数。

`autoconfigure: false` 表示标定由其他工具负责，节点不写参数，但解锁请求仍会转发给固件。

解锁：

```bash
ros2 service call /mecanum_mavlink/arm std_srvs/srv/SetBool "{data: true}"
ros2 service call /mecanum_mavlink/arm std_srvs/srv/SetBool "{data: false}"
```

服务返回成功表示固件 `COMMAND_ACK` 接受；`~/_armed` 与诊断中的 `armed` 跟随固件心跳的解锁位，不自行假设。

## Pi READY 与有序关机

节点是 READY（BCM GPIO17，`ready_gpiochip` 指定 GPIO 字符设备）的唯一所有者：链路心跳新鲜时拉高，心跳失效立即拉低，退出时先拉低再释放。固件把该线作为硬件联锁——READY 为低时拒绝授予 Pi 运动权，因此**整车上 `ready_gpiochip` 不得为空**；为空只用于台架联测。取值需在目标镜像上用 `gpioinfo` 核对。`mecanum_bridge`（自定义协议节点）也占用 READY 与 `/dev/serial0`，两者**不能同时运行**；同时启动时后启动者会因 GPIO 被占用而直接报错退出。

```bash
ros2 service call /mecanum_mavlink/shutdown_pi std_srvs/srv/Trigger '{}'
```

`shutdown_pi` 要求已上锁且心跳新鲜，向固件发送 `COMMAND_LONG(MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN, param1=0, param2=2)` 并等待 `COMMAND_ACK`。固件仅在 Pi 通道、未解锁、无电源禁止时接受，接受后立即禁止所有新授权，PowerTask 等待停轮与内核 HALTED 后单次切电。节点只在接受后拉低 READY，并拒绝重复请求；被拒绝或无 ACK 时 READY 由定时器恢复。ACK 只表示"已接受"，内核停机由 GPIO27 → `gpio-shutdown`/logind 完成，节点不伪造 HALTED。详见 [Pi 部署说明](../../../pi/README.md) 与 [参数持久化与健康保护](../../../docs/参数持久化与健康保护_v1.md)。

## 话题与服务

| 名称 | 类型 | 方向 | 说明 |
| --- | --- | --- | --- |
| `cmd_vel` | `geometry_msgs/Twist` | 订阅 | REP-103：x 前、y 左、绕 z 逆时针为正 |
| `imu/data` | `sensor_msgs/Imu` | 发布 | 50 Hz，BEST_EFFORT；姿态来自固件融合，加速度含重力 |
| `odom` | `nav_msgs/Odometry` | 发布 | 20 Hz，ENU 位姿 + 车体 FLU 速度 |
| `battery` | `sensor_msgs/BatteryState` | 发布 | 电压/V、电流/A、剩余百分比；未知量为 NaN |
| `fix` | `sensor_msgs/NavSatFix` | 发布 | 无定位时 `STATUS_NO_FIX` |
| `rc/channels` | `sensor_msgs/Joy` | 发布 | 前 4 路归一化到 [-1, 1]，其余为开关 |
| `~/armed` | `std_msgs/Bool` | 发布 | 固件解锁状态 |
| `~/statustext` | `std_msgs/String` | 发布 | 固件文本告警 |
| `~/arm` | `std_srvs/SetBool` | 服务 | 解锁/上锁 |
| `~/shutdown_pi` | `std_srvs/Trigger` | 服务 | 请求 Pi 有序关机（需先上锁） |
| `/diagnostics` | `diagnostic_msgs/DiagnosticArray` | 发布 | 链路、遥控、ESP32、电气与运行时状态 |

`cmd_vel` 在 0.5 s（`command_timeout_s`）内持续刷新才生效；超时时若已解锁则持续下发零速度，不会保持旧指令。速度按 `max_speed_mps`、`max_yaw_rate_rad_s` 限幅。仅在标定有效且（已解锁或有新鲜指令）时发送 `SET_POSITION_TARGET_LOCAL_NED`，坐标系 `MAV_FRAME_BODY_NED`，type mask `0x5C7`（保留 vx/vy/vz/yaw_rate，忽略位置、加速度与 yaw）。y 与 yaw_rate 在发送前取反以从 REP-103 转为 NED。

`publish_tf: true` 时发布 `odom → base_link`；IMU 的 frame_id 由 `imu_frame` 指定，需自行提供到 `base_link` 的静态 TF。

## 坐标与单位约定

- 固件输出 NED/FRD。位置 `(n,e,d) → (e,n,-d)`；欧拉角 `(roll, -pitch, π/2 - yaw)`；四元数 `q_enu = q_C ⊗ q_ned ⊗ q_F`，其中 `q_C=(0,√½,√½,0)`、`q_F=(0,1,0,0)`。
- 角速度、机体系线速度 FRD → FLU：`(x, -y, -z)`。
- MAVLink 电池电流为放电正，ROS `BatteryState.current` 为充电正，故取反。
- `HIGHRES_IMU` 上报无重力加速度，节点取反后加上重力语义（静止时 z≈+9.81），与 ROS 约定一致。

## 诊断

`/diagnostics` 包含五个状态块：

- `mecanum_mavlink: serial link`：心跳年龄、PING 往返、`state`、`source`（`pi`/`wifi`）、`fault_bits`、`config_revision`、`pi_link`、`esp_link`、`esp_age_s`、`rc_age_s`、`calibrated`、`armed`、`ready`、`shutdown_pending`。
- `mecanum_mavlink: rc input`：ESP32 SBUS 遥控年龄。
- `mecanum_mavlink: esp32`：ESP32 心跳与年龄。
- `mecanum_mavlink: electrical`：`i_fl_a`…`i_rr_a`、`ntc_c`、`soc`、`stall_mask`、`fault_bits`、`fault_names`。
- `mecanum_mavlink: runtime`：`stack_free_*_words`（7 个任务）、`jitter_last_us`、`jitter_max_us`、`cpu_percent`、`config_seq`、`config_errors`。

心跳超时、`FAILSAFE`/`FAULT` 或标定未生效为 ERROR；ESP32 掉线或遥控超时为 WARN。电气块在过流/堵转/过温/欠压/传感器故障位存在时为 ERROR，电流或 SOC 未上报时为 WARN；运行时块在剩余栈低于 32 字、CPU 超过 80% 或参数保存失败时为 WARN，固件未上报时为 STALE。`state` 为 `DISARMED`、`ARMING`、`WIFI_ACTIVE`、`PI_ACTIVE`、`FAILSAFE`、`FAULT`。

固件侧的换算、阈值与保护逻辑见 [参数持久化与健康保护](../../../docs/参数持久化与健康保护_v1.md)。

## 测试

```bash
source /opt/ros/humble/setup.bash
source ~/.venvs/mecanum-ros2/bin/activate
PYTHONPATH=.:$PYTHONPATH ~/.venvs/mecanum-ros2/bin/python -m unittest Code.tests.test_mavlink_ros -v
```

该测试通过 PTY 启动一个最小飞控模拟器，覆盖握手、参数回显、解锁、`cmd_vel` 转换（含/yaw 取反与 0x5C7 掩码）、IMU/里程计/电池/GNSS/遥控发布、诊断内容、READY 随链路变化与 `shutdown_pi` 的接受/拒绝路径，不访问真实串口，也不触碰真实 GPIO。
