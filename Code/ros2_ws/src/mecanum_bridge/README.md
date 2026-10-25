# Mecanum ROS 2 bridge

面向 ROS 2 Humble，连接 Pi 的 `/dev/serial0`（921600、8N1）与 STM32 USART6。节点启动及重连均保持禁止驱动，需要显式配置和解锁。

## 构建与启动

```bash
source /opt/ros/humble/setup.bash
source ~/.venvs/mecanum-ros2/bin/activate
cd Code/ros2_ws
python -m colcon build --packages-select mecanum_bridge
source install/setup.bash
ros2 run mecanum_bridge bridge --ros-args --params-file src/mecanum_bridge/config/vehicle.yaml
```

先在 YAML 填入实测轮半径、半轴距/轮距、轮端四倍频计数、编码器/电机符号并确认 PI 台架参数。默认几何和计数为零，配置会明确拒绝。

```bash
ros2 service call /mecanum_bridge/clear_fault std_srvs/srv/Trigger '{}'
ros2 service call /mecanum_bridge/configure std_srvs/srv/Trigger '{}'
# 发布连续命令后再明确解锁；示例必须先在悬空台架上验证方向。
ros2 topic pub --rate 50 /cmd_vel geometry_msgs/msg/Twist '{linear: {x: 0.05}}'
ros2 service call /mecanum_bridge/arm std_srvs/srv/SetBool '{data: true}'
ros2 service call /mecanum_bridge/arm std_srvs/srv/SetBool '{data: false}'
```

服务成功意味着收到实际 MCU 响应；解锁还要等 MCU STATUS 确认进入控制态。`cmd_vel` 超过 150 ms 未更新会撤销授权，恢复话题不自动解锁。重启后重新配置 RAM 参数和解锁。服务管理事务超时会返回失败。

## 话题与时间

- 订阅 `/cmd_vel`，车体 x 前、y 左、逆时针角速度为正。
- 发布 `/odom`、`/joint_states` 及可关闭的 `odom → base_link` TF。
- 解码 `/imu/data_raw`、`/imu/temperature`、`/fix`、`/scan`、`/battery`、`/diagnostics`。MCU 采集代码已接入，实际传感器与时序尚需上板验证。
- SYNC 用往返时间估算 MCU/主机单调时间差，取近期最小延迟样本映射到 ROS 时间；尚无同步样本时用接收时间。该方法包含串口/调度误差，不能当作硬件同步。
- IMU 不伪造姿态，orientation_covariance[0]=-1；GNSS 无有效定位时发布 NO_FIX/NaN；未知电池量使用 NaN。
- 雷达按角度回绕组圈；初始半圈、断流残圈会丢弃，缺失分箱为 NaN。`time_increment=0`，当前输出不用于逐点运动去畸变。
- `/rc/sbus_channels_raw` 发布 16 路原始 SBUS 通道（0～2047），仅在新鲜且无丢帧/failsafe/传输异常时更新；`/diagnostics` 的 `mecanum/sbus` 同时报状态。通道未连接运动命令，遥控映射和仲裁仍待实现。
- 传感器诊断包含 `ld06_enable_request`、`aux_pgood`、`ld06_branch_fault_latched`。支路故障为 ERROR；MCU 保持雷达局部电源关闭到重启，当前没有远程清除该锁存的服务。

IMU/GNSS 参考帧需与实际安装方向和 ROS 静态 TF 对齐。轮式里程计协方差为初始估值，打滑与悬架变化需要实测或融合估计。

协议、任务与剩余工作见 `Code/docs/控制链路实现_v1.md`、`Code/docs/传感器采集与遥测_v1.md`。

Pi GPIO 状态已通过 STATUS 上报到 ROS 诊断：`pi_ready`、`pi_halted`、`pi_power_good`、`level_shift_enable_request`、`pi_shutdown_requested`。旧固件未提供有效标记时显示 unknown；后两项表示 MCU 输出请求，READY 输出已接入；Pi 有序关机与切电状态机已接入，完整电池与低功耗管理仍待实现。

Pi 4 部署及 GPIO 权限见 [Code/pi/README.md](../../../pi/README.md)。`ready_gpiochip` 留空用于 PC/PTY；真实车辆需核对 GPIO 控制器并启用 GPIO17。MCU 同时检查 PGOOD、READY 和 HALTED，输入失效撤销 Pi 授权，恢复后必须重新解锁。

新增 `/mecanum_bridge/shutdown_pi`（Trigger）：停车、撤销授权并等待上一事务完成后申请。MCU 等待停轮、内核 HALTED，再单次发出切电脉冲；ACK 只表示接受请求。电源超时不强切、不自动重试。见 `Code/docs/Pi有序关机与切电_v1.md`，实际 A/B 接线及 Pi 波形尚未验证。
