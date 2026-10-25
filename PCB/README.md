# 最终硬件交付

本目录只发布最终版本，历史备份、过程性布局/路由脚本与大体积中间快照保留在开发机。

| 文件 | 用途 |
| --- | --- |
| [完整原生归档](backups/PCB3_1_routing_complete.epro2) | 用嘉立创EDA导入；同时包含本轮核对的 PCB2 控制板与 PCB3_1 电源驱动板 |
| [布线完成复核](docs/PCB3_1_布线完成复核_v1.md) | PCB3_1 3210 线段、335 过孔、816 原始焊盘、234 元件；六层结构、连通性与间距审计 |
| [PCB2 最终 BOM](bom/BOM_PCB2_final.csv) | 从上述归档逐器件提取的 139 条元件记录 |
| [PCB3_1 最终 BOM](bom/BOM_PCB3_1_final.csv) | 从上述归档逐器件提取的 234 条元件记录 |
| [填孔盖帽孔位](bom/PCB3_1_填孔盖帽孔位表_v1.csv) | 制造工艺定位；插件焊接孔不做填孔 |
| [板间契约](design/AB_INTERCONNECT_v1.json) | 60 位信号、8 位电源接口的网络与端点约束 |
| `reports/PCB3_1_routing_complete.png` | 最终布线截图 |
| `reports/pcb3_1_closeout_summary.json` | 最终审计汇总 |
| `reports/pcb3_1_completed_drc.json` | 最终几何 DRC 结果；不是原生 DRC 导出 |
| `reports/pcb3_1_completed_connectivity.json` | 最终信号连通性审计 |
| `reports/pcb3_1_completed_ground_audit.json` | 最终 GND 铜形与异网间距审计 |

本轮代码/电路一致性复核见 [程序复核](../Code/docs/代码与电路一致性复核_v1.md)。原生 DRC 接口未取得有效报告；制板前仍需重新确认最终导出、层叠和装配工艺。焊盘内过孔要求树脂填孔、整平及铜盖帽，普通盖油不能替代。

BOM 按位号逐行导出，保留器件属性中的型号、供应商编号和描述；空字段需要在采购/贴装前核对，不能把无供应商号的机械/非采购对象直接订购。本次未提供已验证的最终 Gerber/钻孔生产包。
