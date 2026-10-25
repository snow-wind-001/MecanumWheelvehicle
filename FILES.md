# 最终发布文件索引

本索引只介绍本次 GitHub 发布文件，不包含本地历史快照、候选布局、旧工程、过程日志、自动备份与编译缓存。第三方 HAL/CMSIS/FreeRTOS/MAVLink 依赖按当前编译工程原目录保留；legacy 小型实现用于现有兼容性测试，不是另一份产品快照。

机械源文件依赖名称和相对路径，不应在下载后随意重命名。文档与机器报告所述“通过”均按各自检查范围解释；没有实板测试结果。

## 根目录

| 文件 | 用途 |
| --- | --- |
| [.gitignore](.gitignore) | 精简最终发布白名单及本机/构建文件排除规则 |
| [AGENTS.md](AGENTS.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [FILES.md](FILES.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [README.md](README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |

## 3D

| 文件 | 用途 |
| --- | --- |
| [3D/2020 Aluminum profile 100mm.SLDPRT](3D/2020%20Aluminum%20profile%20100mm.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/2020 Aluminum profile 160mm.SLDPRT](3D/2020%20Aluminum%20profile%20160mm.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/2020 Aluminum profile 200mm.SLDPRT](3D/2020%20Aluminum%20profile%20200mm.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/2020 Aluminum profile 60mm.SLDPRT](3D/2020%20Aluminum%20profile%2060mm.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/2020 Corner pieces.SLDPRT](3D/2020%20Corner%20pieces.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Axle of Hinge.SLDPRT](3D/Axle%20of%20Hinge.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Flange.SLDPRT](3D/Flange.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Hinge 1.SLDPRT](3D/Hinge%201.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Hinge 2.SLDPRT](3D/Hinge%202.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Hinge assemabled.SLDASM](3D/Hinge%20assemabled.SLDASM) | SolidWorks 总装或子装配；保留关联零件路径 |
| [3D/Hinge pad.SLDPRT](3D/Hinge%20pad.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/IMG_20170404_210102_AO_HDR.jpg](3D/IMG_20170404_210102_AO_HDR.jpg) | 实物装配照片与安装参考 |
| [3D/IMG_20170404_214613_AO_HDR.jpg](3D/IMG_20170404_214613_AO_HDR.jpg) | 实物装配照片与安装参考 |
| [3D/IMG_20170406_211856_AO_HDR.jpg](3D/IMG_20170406_211856_AO_HDR.jpg) | 实物装配照片与安装参考 |
| [3D/IMG_20170409_224633_AO_HDR.jpg](3D/IMG_20170409_224633_AO_HDR.jpg) | 实物装配照片与安装参考 |
| [3D/IMG_20170523_214143_AO_HDR.jpg](3D/IMG_20170523_214143_AO_HDR.jpg) | 实物装配照片与安装参考 |
| [3D/M3 Nut.SLDPRT](3D/M3%20Nut.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/M3x72mm Screw.SLDPRT](3D/M3x72mm%20Screw.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Mecanum wheel assembled.SLDASM](3D/Mecanum%20wheel%20assembled.SLDASM) | SolidWorks 总装或子装配；保留关联零件路径 |
| [3D/Motor.SLDPRT](3D/Motor.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Roller.SLDPRT](3D/Roller.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Shock absorber.SLDPRT](3D/Shock%20absorber.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Swing arm assembled.SLDASM](3D/Swing%20arm%20assembled.SLDASM) | SolidWorks 总装或子装配；保留关联零件路径 |
| [3D/Swing arm.SLDPRT](3D/Swing%20arm.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Top absorber frame.SLDPRT](3D/Top%20absorber%20frame.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |
| [3D/Vehicle Assembled (1).STEP](3D/Vehicle%20Assembled%20%281%29.STEP) | 整车中性几何导出，供跨 CAD 工具读取 |
| [3D/Vehicle Assembled.SLDASM](3D/Vehicle%20Assembled.SLDASM) | SolidWorks 总装或子装配；保留关联零件路径 |
| [3D/Vehicle Layout v15.blend](3D/Vehicle%20Layout%20v15.blend) | 当前提供的 Blender v15 布局场景；本轮未重新验证机械干涉 |
| [3D/Wheel hub.SLDPRT](3D/Wheel%20hub.SLDPRT) | SolidWorks 机械零件；装配按文件名引用 |

## Code · ESP32 当前工程

| 文件 | 用途 |
| --- | --- |
| [Code/ESP32_Radio_Bridge/README.md](Code/ESP32_Radio_Bridge/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/ESP32_Radio_Bridge/platformio.ini](Code/ESP32_Radio_Bridge/platformio.ini) | ESP32 固定平台、引脚、波特率及开发网络参数 |
| [Code/ESP32_Radio_Bridge/src/main.cpp](Code/ESP32_Radio_Bridge/src/main.cpp) | 当前工程配套配置或资源文件 |

## Code · STM32 当前工程

| 文件 | 用途 |
| --- | --- |
| [Code/MecanumB_Control/.cproject](Code/MecanumB_Control/.cproject) | 当前工程配套配置或资源文件 |
| [Code/MecanumB_Control/.mxproject](Code/MecanumB_Control/.mxproject) | 当前工程配套配置或资源文件 |
| [Code/MecanumB_Control/.project](Code/MecanumB_Control/.project) | 当前工程配套配置或资源文件 |
| [Code/MecanumB_Control/.settings/org.eclipse.core.resources.prefs](Code/MecanumB_Control/.settings/org.eclipse.core.resources.prefs) | 当前工程配套配置或资源文件 |
| [Code/MecanumB_Control/App/include/vehicle_app.h](Code/MecanumB_Control/App/include/vehicle_app.h) | RTOS/HAL 编排、输出、Flash 维护和运行统计公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_control.h](Code/MecanumB_Control/App/include/vehicle_control.h) | 麦轮运动学、编码器与轮速 PI公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_fusion.h](Code/MecanumB_Control/App/include/vehicle_fusion.h) | 姿态估计和轮速辅助公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_health.h](Code/MecanumB_Control/App/include/vehicle_health.h) | ADC 换算、健康保护与持续恢复计时公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_link.h](Code/MecanumB_Control/App/include/vehicle_link.h) | 运动授权、租约、故障与维护联锁公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_mavlink.h](Code/MecanumB_Control/App/include/vehicle_mavlink.h) | MAVLink 双通道、参数、遥测、SBUS 与关机公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_power.h](Code/MecanumB_Control/App/include/vehicle_power.h) | Pi 停轮、内核 HALTED 与切电状态机公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_protocol.h](Code/MecanumB_Control/App/include/vehicle_protocol.h) | legacy 帧格式、CRC 与编解码兼容核心公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_sensor_app.h](Code/MecanumB_Control/App/include/vehicle_sensor_app.h) | HAL 传感器采集与任务、DMA 交接公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_sensors.h](Code/MecanumB_Control/App/include/vehicle_sensors.h) | UBX/LD06/SBUS 解析与传感器状态公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_store.h](Code/MecanumB_Control/App/include/vehicle_store.h) | 参数 A/B Flash 追加日志与损坏恢复公共接口 |
| [Code/MecanumB_Control/App/include/vehicle_tx.h](Code/MecanumB_Control/App/include/vehicle_tx.h) | 有限容量遥测/应答队列公共接口 |
| [Code/MecanumB_Control/App/src/vehicle_app.c](Code/MecanumB_Control/App/src/vehicle_app.c) | RTOS/HAL 编排、输出、Flash 维护和运行统计实现 |
| [Code/MecanumB_Control/App/src/vehicle_control.c](Code/MecanumB_Control/App/src/vehicle_control.c) | 麦轮运动学、编码器与轮速 PI实现 |
| [Code/MecanumB_Control/App/src/vehicle_fusion.c](Code/MecanumB_Control/App/src/vehicle_fusion.c) | 姿态估计和轮速辅助实现 |
| [Code/MecanumB_Control/App/src/vehicle_health.c](Code/MecanumB_Control/App/src/vehicle_health.c) | ADC 换算、健康保护与持续恢复计时实现 |
| [Code/MecanumB_Control/App/src/vehicle_link.c](Code/MecanumB_Control/App/src/vehicle_link.c) | 运动授权、租约、故障与维护联锁实现 |
| [Code/MecanumB_Control/App/src/vehicle_mavlink.c](Code/MecanumB_Control/App/src/vehicle_mavlink.c) | MAVLink 双通道、参数、遥测、SBUS 与关机实现 |
| [Code/MecanumB_Control/App/src/vehicle_power.c](Code/MecanumB_Control/App/src/vehicle_power.c) | Pi 停轮、内核 HALTED 与切电状态机实现 |
| [Code/MecanumB_Control/App/src/vehicle_protocol.c](Code/MecanumB_Control/App/src/vehicle_protocol.c) | legacy 帧格式、CRC 与编解码兼容核心实现 |
| [Code/MecanumB_Control/App/src/vehicle_sensor_app.c](Code/MecanumB_Control/App/src/vehicle_sensor_app.c) | HAL 传感器采集与任务、DMA 交接实现 |
| [Code/MecanumB_Control/App/src/vehicle_sensors.c](Code/MecanumB_Control/App/src/vehicle_sensors.c) | UBX/LD06/SBUS 解析与传感器状态实现 |
| [Code/MecanumB_Control/App/src/vehicle_store.c](Code/MecanumB_Control/App/src/vehicle_store.c) | 参数 A/B Flash 追加日志与损坏恢复实现 |
| [Code/MecanumB_Control/App/src/vehicle_tx.c](Code/MecanumB_Control/App/src/vehicle_tx.c) | 有限容量遥测/应答队列实现 |
| [Code/MecanumB_Control/Inc/FreeRTOSConfig.h](Code/MecanumB_Control/Inc/FreeRTOSConfig.h) | 当前固件公共定义、外设或中断接口 |
| [Code/MecanumB_Control/Inc/main.h](Code/MecanumB_Control/Inc/main.h) | 当前固件公共定义、外设或中断接口 |
| [Code/MecanumB_Control/Inc/stm32f4xx_hal_conf.h](Code/MecanumB_Control/Inc/stm32f4xx_hal_conf.h) | 当前固件公共定义、外设或中断接口 |
| [Code/MecanumB_Control/Inc/stm32f4xx_it.h](Code/MecanumB_Control/Inc/stm32f4xx_it.h) | 当前固件公共定义、外设或中断接口 |
| [Code/MecanumB_Control/Makefile](Code/MecanumB_Control/Makefile) | 当前工程构建规则 |
| [Code/MecanumB_Control/MecanumB_Control.ioc](Code/MecanumB_Control/MecanumB_Control.ioc) | 当前 STM32 CubeMX 配置；与实际源码核对后再生成 |
| [Code/MecanumB_Control/STM32F429ZGTX_FLASH.ld](Code/MecanumB_Control/STM32F429ZGTX_FLASH.ld) | 链接布局：程序 768 KiB，参数区独立保留 |
| [Code/MecanumB_Control/STM32F429ZGTX_RAM.ld](Code/MecanumB_Control/STM32F429ZGTX_RAM.ld) | 链接布局：程序 768 KiB，参数区独立保留 |
| [Code/MecanumB_Control/Src/freertos.c](Code/MecanumB_Control/Src/freertos.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/main.c](Code/MecanumB_Control/Src/main.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/stm32f4xx_hal_msp.c](Code/MecanumB_Control/Src/stm32f4xx_hal_msp.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/stm32f4xx_hal_timebase_tim.c](Code/MecanumB_Control/Src/stm32f4xx_hal_timebase_tim.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/stm32f4xx_it.c](Code/MecanumB_Control/Src/stm32f4xx_it.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/syscalls.c](Code/MecanumB_Control/Src/syscalls.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/sysmem.c](Code/MecanumB_Control/Src/sysmem.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Src/system_stm32f4xx.c](Code/MecanumB_Control/Src/system_stm32f4xx.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/MecanumB_Control/Startup/startup_stm32f429zgtx.s](Code/MecanumB_Control/Startup/startup_stm32f429zgtx.s) | STM32F429ZG 启动汇编与向量表 |

## Code · STM32 上游 HAL / CMSIS

| 文件 | 用途 |
| --- | --- |
| [Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f429xx.h](Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f429xx.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f4xx.h](Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f4xx.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/system_stm32f4xx.h](Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/Include/system_stm32f4xx.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt](Code/MecanumB_Control/Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cachel1_armv7.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cachel1_armv7.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armcc.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armcc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armclang.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armclang.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armclang_ltm.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_armclang_ltm.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_compiler.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_compiler.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_gcc.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_gcc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_iccarm.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_iccarm.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_version.h](Code/MecanumB_Control/Drivers/CMSIS/Include/cmsis_version.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv81mml.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv81mml.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv8mbl.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv8mbl.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv8mml.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_armv8mml.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm0.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm0.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm0plus.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm0plus.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm1.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm1.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm23.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm23.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm3.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm3.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm33.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm33.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm35p.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm35p.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm4.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm4.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm55.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm55.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm7.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm7.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm85.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_cm85.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_sc000.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_sc000.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_sc300.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_sc300.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/core_starmc1.h](Code/MecanumB_Control/Drivers/CMSIS/Include/core_starmc1.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/mpu_armv7.h](Code/MecanumB_Control/Drivers/CMSIS/Include/mpu_armv7.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/mpu_armv8.h](Code/MecanumB_Control/Drivers/CMSIS/Include/mpu_armv8.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/pac_armv81.h](Code/MecanumB_Control/Drivers/CMSIS/Include/pac_armv81.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/pmu_armv8.h](Code/MecanumB_Control/Drivers/CMSIS/Include/pmu_armv8.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/Include/tz_context.h](Code/MecanumB_Control/Drivers/CMSIS/Include/tz_context.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/CMSIS/LICENSE.txt](Code/MecanumB_Control/Drivers/CMSIS/LICENSE.txt) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_adc.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_adc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_adc_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_adc_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_cortex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_cortex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_def.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_def.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_exti.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_exti.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ramfunc.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash_ramfunc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_gpio_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_i2c_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_iwdg.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_iwdg.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_pwr_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rcc_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rng.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_rng.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_spi.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim_ex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_tim_ex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_adc.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_adc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_bus.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_bus.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_cortex.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_cortex.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_dma.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_dma.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_exti.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_exti.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_gpio.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_gpio.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_i2c.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_i2c.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_iwdg.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_iwdg.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_pwr.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_pwr.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rcc.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rcc.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_spi.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_spi.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_system.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_system.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_tim.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_tim.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_utils.h](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_utils.h) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/LICENSE.txt](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/LICENSE.txt) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rng.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rng.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |
| [Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c](Code/MecanumB_Control/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c) | 上游 HAL / CMSIS 源码、接口或许可证，按原始目录保留 |

## Code · FreeRTOS / MAVLink 上游依赖

| 文件 | 用途 |
| --- | --- |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/LICENSE](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/LICENSE) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/croutine.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/croutine.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/StackMacros.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/StackMacros.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/atomic.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/atomic.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/croutine.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/croutine.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/deprecated_definitions.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/deprecated_definitions.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/event_groups.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/event_groups.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/list.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/message_buffer.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/message_buffer.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/mpu_prototypes.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/mpu_prototypes.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/mpu_wrappers.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/mpu_wrappers.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/portable.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/portable.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/projdefs.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/projdefs.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/queue.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/queue.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/stack_macros.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/stack_macros.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/stream_buffer.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/stream_buffer.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/task.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/task.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/timers.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/include/timers.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/list.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/list.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/portmacro.h](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/portmacro.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/queue.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/queue.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/tasks.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/tasks.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/timers.c](Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/timers.c) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/checksum.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/checksum.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/common.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/common.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_actuator_control_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_actuator_control_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_actuator_output_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_actuator_output_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_adsb_vehicle.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_adsb_vehicle.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_airspeed.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_airspeed.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_ais_vessel.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_ais_vessel.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_altitude.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_altitude.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_att_pos_mocap.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_att_pos_mocap.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_quaternion.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_quaternion.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_quaternion_cov.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_quaternion_cov.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_attitude_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_auth_key.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_auth_key.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_autopilot_state_for_gimbal_device.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_autopilot_state_for_gimbal_device.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_available_modes.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_available_modes.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_available_modes_monitor.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_available_modes_monitor.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_battery_info.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_battery_info.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_battery_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_battery_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_button_change.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_button_change.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_capture_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_capture_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_fov_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_fov_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_image_captured.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_image_captured.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_settings.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_settings.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_thermal_range.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_thermal_range.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_tracking_geo_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_tracking_geo_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_tracking_image_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_tracking_image_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_trigger.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_camera_trigger.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_can_filter_modify.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_can_filter_modify.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_can_frame.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_can_frame.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_canfd_frame.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_canfd_frame.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_cellular_config.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_cellular_config.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_cellular_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_cellular_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_change_operator_control.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_change_operator_control.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_change_operator_control_ack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_change_operator_control_ack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_collision.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_collision.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_ack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_ack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_cancel.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_cancel.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_long.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_command_long.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_information_basic.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_information_basic.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_metadata.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_component_metadata.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_control_system_state.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_control_system_state.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_current_event_sequence.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_current_event_sequence.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_current_mode.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_current_mode.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_data_stream.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_data_stream.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_data_transmission_handshake.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_data_transmission_handshake.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug_float_array.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug_float_array.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug_vect.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_debug_vect.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_distance_sensor.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_distance_sensor.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_efi_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_efi_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_encapsulated_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_encapsulated_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_esc_info.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_esc_info.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_esc_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_esc_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_estimator_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_estimator_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_event.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_event.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_extended_sys_state.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_extended_sys_state.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_fence_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_fence_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_figure_eight_execution_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_figure_eight_execution_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_file_transfer_protocol.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_file_transfer_protocol.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_flight_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_flight_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_follow_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_follow_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_fuel_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_fuel_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_generator_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_generator_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_attitude_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_attitude_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_set_attitude.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_device_set_attitude.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_attitude.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_attitude.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_manual_control.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_manual_control.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_pitchyaw.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_set_pitchyaw.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gimbal_manager_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_position_int_cov.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_position_int_cov.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_position_sensor.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_position_sensor.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_vision_position_estimate.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_global_vision_position_estimate.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps2_raw.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps2_raw.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps2_rtk.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps2_rtk.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_global_origin.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_global_origin.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_inject_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_inject_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_input.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_input.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_raw_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_raw_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_rtcm_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_rtcm_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_rtk.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_rtk.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_gps_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_high_latency.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_high_latency.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_high_latency2.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_high_latency2.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_highres_imu.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_highres_imu.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_actuator_controls.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_actuator_controls.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_controls.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_controls.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_gps.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_gps.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_optical_flow.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_optical_flow.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_rc_inputs_raw.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_rc_inputs_raw.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_sensor.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_sensor.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_state.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_state.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_state_quaternion.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hil_state_quaternion.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_home_position.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_home_position.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hygrometer_sensor.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_hygrometer_sensor.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_illuminator_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_illuminator_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_isbd_link_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_isbd_link_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_landing_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_landing_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_link_node_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_link_node_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned_cov.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned_cov.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned_system_global_offset.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_local_position_ned_system_global_offset.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_entry.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_entry.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_erase.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_erase.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_end.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_end.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_log_request_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_ack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_ack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_data_acked.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_logging_data_acked.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mag_cal_report.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mag_cal_report.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_manual_control.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_manual_control.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_manual_setpoint.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_manual_setpoint.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_memory_vect.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_memory_vect.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_message_interval.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_message_interval.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_ack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_ack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_clear_all.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_clear_all.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_count.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_count.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_current.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_current.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item_reached.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_item_reached.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_partial_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_request_partial_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_set_current.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_set_current.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_write_partial_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mission_write_partial_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mount_orientation.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_mount_orientation.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_named_value_float.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_named_value_float.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_named_value_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_named_value_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_nav_controller_output.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_nav_controller_output.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_obstacle_distance.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_obstacle_distance.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_odometry.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_odometry.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_onboard_computer_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_onboard_computer_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_arm_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_arm_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_authentication.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_authentication.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_basic_id.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_basic_id.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_location.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_location.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_message_pack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_message_pack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_operator_id.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_operator_id.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_self_id.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_self_id.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_system.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_system.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_system_update.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_open_drone_id_system_update.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_optical_flow.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_optical_flow.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_optical_flow_rad.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_optical_flow_rad.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_orbit_execution_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_orbit_execution_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_error.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_error.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_ack.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_ack.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_request_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_request_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_request_read.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_request_read.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_set.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_set.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_value.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_ext_value.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_map_rc.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_map_rc.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_request_list.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_request_list.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_request_read.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_request_read.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_set.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_set.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_value.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_param_value.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_ping.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_ping.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_play_tune.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_play_tune.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_play_tune_v2.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_play_tune_v2.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_position_target_global_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_position_target_global_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_position_target_local_ned.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_position_target_local_ned.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_power_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_power_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_protocol_version.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_protocol_version.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_radio_rc_channels.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_radio_rc_channels.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_radio_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_radio_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_imu.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_imu.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_pressure.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_pressure.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_rpm.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_raw_rpm.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_override.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_override.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_raw.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_raw.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_scaled.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_rc_channels_scaled.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_relay_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_relay_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_request_data_stream.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_request_data_stream.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_request_event.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_request_event.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_resource_request.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_resource_request.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_response_event_error.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_response_event_error.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_safety_allowed_area.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_safety_allowed_area.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_safety_set_allowed_area.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_safety_set_allowed_area.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu2.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu2.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu3.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_imu3.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure2.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure2.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure3.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_scaled_pressure3.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_serial_control.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_serial_control.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_servo_output_raw.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_servo_output_raw.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_actuator_control_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_actuator_control_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_attitude_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_attitude_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_gps_global_origin.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_gps_global_origin.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_home_position.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_home_position.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_mode.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_mode.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_position_target_global_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_position_target_global_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_position_target_local_ned.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_set_position_target_local_ned.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_setup_signing.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_setup_signing.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_sim_state.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_sim_state.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_smart_battery_info.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_smart_battery_info.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_statustext.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_statustext.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_storage_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_storage_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_supported_tunes.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_supported_tunes.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_sys_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_sys_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_system_time.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_system_time.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_check.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_check.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_data.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_data.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_report.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_report.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_request.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_terrain_request.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_time_estimate_to_target.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_time_estimate_to_target.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_timesync.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_timesync.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_trajectory_representation_bezier.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_trajectory_representation_bezier.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_trajectory_representation_waypoints.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_trajectory_representation_waypoints.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_tunnel.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_tunnel.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_uavcan_node_info.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_uavcan_node_info.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_uavcan_node_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_uavcan_node_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_utm_global_position.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_utm_global_position.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_v2_extension.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_v2_extension.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vfr_hud.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vfr_hud.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vibration.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vibration.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vicon_position_estimate.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vicon_position_estimate.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_video_stream_information.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_video_stream_information.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_video_stream_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_video_stream_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vision_position_estimate.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vision_position_estimate.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vision_speed_estimate.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_vision_speed_estimate.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wheel_distance.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wheel_distance.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wifi_config_ap.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wifi_config_ap.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_winch_status.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_winch_status.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wind_cov.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/mavlink_msg_wind_cov.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/testsuite.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/testsuite.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/version.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/common/version.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_conversions.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_conversions.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_get_info.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_get_info.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_helpers.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_helpers.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_sha256.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_sha256.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_types.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/mavlink_types.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/mavlink.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/mavlink.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/mavlink_msg_heartbeat.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/mavlink_msg_heartbeat.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/minimal.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/minimal.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/testsuite.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/testsuite.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/version.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/minimal/version.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/protocol.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/protocol.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink_msg_autopilot_version.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink_msg_autopilot_version.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink_msg_global_position_int.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/mavlink_msg_global_position_int.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/standard.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/standard.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/testsuite.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/testsuite.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |
| [Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/version.h](Code/MecanumB_Control/Middlewares/Third_Party/MAVLink/standard/version.h) | 当前构建使用的 FreeRTOS / MAVLink 上游代码或许可证 |

## Code · 文档、部署、测试与检查工具

| 文件 | 用途 |
| --- | --- |
| [Code/README.md](Code/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/cubemx/B板引脚分配_v1.csv](Code/cubemx/B%E6%9D%BF%E5%BC%95%E8%84%9A%E5%88%86%E9%85%8D_v1.csv) | 引脚或制造工艺的结构化清单；具体用途见文件名/表头 |
| [Code/docs/B板引脚定义_v1.md](Code/docs/B%E6%9D%BF%E5%BC%95%E8%84%9A%E5%AE%9A%E4%B9%89_v1.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/docs/FreeRTOS程序报告_v1.html](Code/docs/FreeRTOS%E7%A8%8B%E5%BA%8F%E6%8A%A5%E5%91%8A_v1.html) | 更新后的离线 FreeRTOS 程序与电路一致性详细报告 |
| [Code/docs/代码与电路一致性复核_v1.md](Code/docs/%E4%BB%A3%E7%A0%81%E4%B8%8E%E7%94%B5%E8%B7%AF%E4%B8%80%E8%87%B4%E6%80%A7%E5%A4%8D%E6%A0%B8_v1.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/docs/参数持久化与健康保护_v1.md](Code/docs/%E5%8F%82%E6%95%B0%E6%8C%81%E4%B9%85%E5%8C%96%E4%B8%8E%E5%81%A5%E5%BA%B7%E4%BF%9D%E6%8A%A4_v1.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/pi/60-mecanum-gpio.rules](Code/pi/60-mecanum-gpio.rules) | Pi GPIO/串口权限或系统电源配置模板 |
| [Code/pi/60-mecanum-power.conf](Code/pi/60-mecanum-power.conf) | Pi GPIO/串口权限或系统电源配置模板 |
| [Code/pi/README.md](Code/pi/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/pi/config-halted-pending.txt](Code/pi/config-halted-pending.txt) | 待实板验证的内核 HALTED overlay 模板，不能视为已启用 |
| [Code/pi/config-uart-shutdown.txt](Code/pi/config-uart-shutdown.txt) | 当前工程配套配置或资源文件 |
| [Code/pi/mecanum-bridge.service](Code/pi/mecanum-bridge.service) | Pi systemd 主服务；启动 mecanum_mavlink |
| [Code/reports/firmware_image_contract_v1.json](Code/reports/firmware_image_contract_v1.json) | 最终 ELF SRAM 擦除与参数区边界检查结果 |
| [Code/reports/hardware_contract_v1.json](Code/reports/hardware_contract_v1.json) | 最终归档 MCU/板间接口对照及来源摘要 |
| [Code/reports/verification_final_v1.json](Code/reports/verification_final_v1.json) | 最终 STM32/ESP32/ROS/80 项测试验证汇总 |
| [Code/tests/mavlink_fixture.c](Code/tests/mavlink_fixture.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/tests/native_fixture.c](Code/tests/native_fixture.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/tests/native_tests.c](Code/tests/native_tests.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/tests/store_health_fixture.c](Code/tests/store_health_fixture.c) | 当前 C 源码或原生测试夹具；职责以文件名和接口为准 |
| [Code/tests/test_mavlink.py](Code/tests/test_mavlink.py) | MAVLink v2 adapter and attitude-estimator tests against the production C code. |
| [Code/tests/test_mavlink_ros.py](Code/tests/test_mavlink_ros.py) | MAVLink ROS 2 bridge tests: pure conversions plus a PTY integration run. |
| [Code/tests/test_pi_gpio.py](Code/tests/test_pi_gpio.py) | Check Pi READY ownership and cleanup without accessing host GPIO hardware. |
| [Code/tests/test_power.py](Code/tests/test_power.py) | Exercise production Pi power sequencing with simulated physical inputs. |
| [Code/tests/test_ros_bridge.py](Code/tests/test_ros_bridge.py) | Real ROS service/topic smoke test over PTY using the production C MCU core. |
| [Code/tests/test_sensors.py](Code/tests/test_sensors.py) | Sensor binary vectors, C parser interoperability and LD06 scan boundaries. |
| [Code/tests/test_storage_health.py](Code/tests/test_storage_health.py) | A/B parameter log and electrical-health tests against the production C code. |
| [Code/tests/test_vehicle.py](Code/tests/test_vehicle.py) | Exercise the Python host against compiled production STM32 protocol/state code. |
| [Code/tools/check_clock_config.py](Code/tools/check_clock_config.py) | Reject inconsistent generated STM32F429 PLL clocks before compiling. |
| [Code/tools/check_environment.py](Code/tools/check_environment.py) | Check local development prerequisites without moving or powering hardware. |
| [Code/tools/check_firmware_image.py](Code/tools/check_firmware_image.py) | Check the linked SRAM erase function and reserved parameter sectors. |
| [Code/tools/check_hardware_contract.py](Code/tools/check_hardware_contract.py) | Compare the archived PCB2 MCU nets with firmware pin definitions. |
| [Code/tools/render_freertos_report.py](Code/tools/render_freertos_report.py) | Render the final offline FreeRTOS engineering report from checked evidence. |

## Code · ROS 2 节点与兼容测试实现

| 文件 | 用途 |
| --- | --- |
| [Code/ros2_ws/src/mecanum_bridge/README.md](Code/ros2_ws/src/mecanum_bridge/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/ros2_ws/src/mecanum_bridge/config/vehicle.yaml](Code/ros2_ws/src/mecanum_bridge/config/vehicle.yaml) | ROS 节点、启动或部署参数模板 |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/__init__.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/__init__.py) | ROS 2 host support for the Mecanum dual-board vehicle. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/client.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/client.py) | Transport-independent host session, authorization and command deadlines. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/lidar.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/lidar.py) | Validate LD06 raw batches and assemble fresh clockwise revolutions for ROS. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/node.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/node.py) | ROS 2 node for the dual-board vehicle's protocol v1 serial link. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/odometry.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/odometry.py) | Integrate normalized X-roller wheel counts in ROS x-forward/y-left coordinates. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/pi_gpio.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/pi_gpio.py) | Own Pi GPIO17 READY through the Linux GPIO character device. |
| [Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/protocol.py](Code/ros2_ws/src/mecanum_bridge/mecanum_bridge/protocol.py) | Bounded protocol v1 codec shared with the STM32 wire contract. |
| [Code/ros2_ws/src/mecanum_bridge/package.xml](Code/ros2_ws/src/mecanum_bridge/package.xml) | ROS 或 IDE 工程元数据 |
| [Code/ros2_ws/src/mecanum_bridge/resource/mecanum_bridge](Code/ros2_ws/src/mecanum_bridge/resource/mecanum_bridge) | ROS ament 包发现标记 |
| [Code/ros2_ws/src/mecanum_bridge/setup.cfg](Code/ros2_ws/src/mecanum_bridge/setup.cfg) | 当前工程配套配置或资源文件 |
| [Code/ros2_ws/src/mecanum_bridge/setup.py](Code/ros2_ws/src/mecanum_bridge/setup.py) | Python 节点、部署适配、测试或工程检查实现 |
| [Code/ros2_ws/src/mecanum_mavlink/README.md](Code/ros2_ws/src/mecanum_mavlink/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [Code/ros2_ws/src/mecanum_mavlink/config/vehicle.yaml](Code/ros2_ws/src/mecanum_mavlink/config/vehicle.yaml) | ROS 节点、启动或部署参数模板 |
| [Code/ros2_ws/src/mecanum_mavlink/launch/bridge.launch.py](Code/ros2_ws/src/mecanum_mavlink/launch/bridge.launch.py) | Launch the MAVLink v2 bridge with the vehicle calibration file. |
| [Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/__init__.py](Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/__init__.py) | MAVLink v2 <-> ROS2 bridge for the MecanumB vehicle. |
| [Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/bridge.py](Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/bridge.py) | ROS2 bridge between the MecanumB STM32 firmware and a MAVLink v2 companion link. |
| [Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/conversions.py](Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/conversions.py) | Frame and unit conversions between MAVLink (NED/FRD) and ROS REP-103 (ENU/FLU). |
| [Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/pi_gpio.py](Code/ros2_ws/src/mecanum_mavlink/mecanum_mavlink/pi_gpio.py) | Own Pi GPIO17 READY through the Linux GPIO character device. |
| [Code/ros2_ws/src/mecanum_mavlink/package.xml](Code/ros2_ws/src/mecanum_mavlink/package.xml) | ROS 或 IDE 工程元数据 |
| [Code/ros2_ws/src/mecanum_mavlink/resource/mecanum_mavlink](Code/ros2_ws/src/mecanum_mavlink/resource/mecanum_mavlink) | ROS ament 包发现标记 |
| [Code/ros2_ws/src/mecanum_mavlink/setup.cfg](Code/ros2_ws/src/mecanum_mavlink/setup.cfg) | 当前工程配套配置或资源文件 |
| [Code/ros2_ws/src/mecanum_mavlink/setup.py](Code/ros2_ws/src/mecanum_mavlink/setup.py) | Python 节点、部署适配、测试或工程检查实现 |

## PCB

| 文件 | 用途 |
| --- | --- |
| [PCB/README.md](PCB/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [PCB/backups/PCB3_1_routing_complete.epro2](PCB/backups/PCB3_1_routing_complete.epro2) | 本次唯一发布的完整 EDA 最终原生归档 |
| [PCB/bom/BOM_PCB2_final.csv](PCB/bom/BOM_PCB2_final.csv) | 从最终原生工程逐器件导出的 BOM |
| [PCB/bom/BOM_PCB3_1_final.csv](PCB/bom/BOM_PCB3_1_final.csv) | 从最终原生工程逐器件导出的 BOM |
| [PCB/bom/PCB3_1_填孔盖帽孔位表_v1.csv](PCB/bom/PCB3_1_%E5%A1%AB%E5%AD%94%E7%9B%96%E5%B8%BD%E5%AD%94%E4%BD%8D%E8%A1%A8_v1.csv) | 引脚或制造工艺的结构化清单；具体用途见文件名/表头 |
| [PCB/design/AB_INTERCONNECT_v1.json](PCB/design/AB_INTERCONNECT_v1.json) | A/B 60 位信号与 8 位电源接口契约 |
| [PCB/docs/PCB3_1_布线完成复核_v1.md](PCB/docs/PCB3_1_%E5%B8%83%E7%BA%BF%E5%AE%8C%E6%88%90%E5%A4%8D%E6%A0%B8_v1.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [PCB/reports/PCB3_1_routing_complete.png](PCB/reports/PCB3_1_routing_complete.png) | 最终 PCB3_1 布线截图 |
| [PCB/reports/pcb3_1_closeout_summary.json](PCB/reports/pcb3_1_closeout_summary.json) | 最终 PCB 几何、信号或 GND 检查结果；范围见内容 |
| [PCB/reports/pcb3_1_completed_connectivity.json](PCB/reports/pcb3_1_completed_connectivity.json) | 最终 PCB 几何、信号或 GND 检查结果；范围见内容 |
| [PCB/reports/pcb3_1_completed_drc.json](PCB/reports/pcb3_1_completed_drc.json) | 最终 PCB 几何、信号或 GND 检查结果；范围见内容 |
| [PCB/reports/pcb3_1_completed_ground_audit.json](PCB/reports/pcb3_1_completed_ground_audit.json) | 最终 PCB 几何、信号或 GND 检查结果；范围见内容 |

## tools

| 文件 | 用途 |
| --- | --- |
| [tools/README.md](tools/README.md) | 使用说明、设计复核、文件索引或贡献约定（以文件标题为准） |
| [tools/step_to_obj.py](tools/step_to_obj.py) | Convert a STEP assembly into an OBJ mesh that Blender can import. |

## 程序报告交互资源

以下是当前报告的源资源，生成时内嵌到单个 HTML；不需要在线脚本或图形服务。

| 文件 | 用途 |
| --- | --- |
| [interactive.html](Code/tools/report_assets/interactive.html) | 系统结构 SVG、流程播放器、麦轮示意与 PI 控件的结构和图文说明 |
| [interactive.css](Code/tools/report_assets/interactive.css) | 图表、步骤高亮、轮组方向动画、窄屏/打印及减少动态效果样式 |
| [interactive.js](Code/tools/report_assets/interactive.js) | 四类流程与故障分支、播放/单步状态、模块选择、麦轮逆解/正解和单步 PI 演示 |
