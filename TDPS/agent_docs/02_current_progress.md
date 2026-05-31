# 当前项目进度

## 已实现内容

- 巡线：8 路灰度传感器处理、标定、归一化、滤波、PID、直道高速、弯前减速、箭头/宽黑干扰保护、可信方向丢线恢复和岔路识别。
- 雷达：HLK-LD2410S 串口帧解析、`CLEAR/WARN/BLOCK` 状态输出、WARN 减速、BLOCK 开环绕障与重新寻线。
- 无线：EWM22A LoRa 异步队列、超时重试、可选 ACK、检查点事件入口 `LF_App_NotifyCheckpoint(checkpoint_id)`。
- 测试：PC stub、CMake/gcc 离线测试、Simulator 回归、板级测试程序和独立雷达参考程序。

## 里程碑状态

来源：`TDPS/docs/basic_design.md` 和现有调研记录。

| 里程碑 | 内容 | 当前状态 |
|---|---|---|
| M1 | 底盘与外设联通 | 已完成 / 待用最新实物状态确认 |
| M2 | 基础巡线闭环 | 已完成 / 离线可验证 |
| M3 | 丢线恢复稳定 | on track |
| M4 | LoRa 窗口发送接入 | on track |
| M5 | 雷达决策接入 | 已有实现，待实机确认 |
| M6 | 三任务统一联调 | 待全车实测 |

## 仿真结果记录

来源：`TDPS/docs/tuning.md`。

- 最新 track quick normal：16 个场景 High。
- `overall_score=95.41`
- `avg_detect=100.00%`
- `max_lost=0.000s`
- 此前压力稳定性：5 个 seed 均为 High，`min_score=94.45`，`min_detect=99.95%`，`max_lost=0.160s`。

## 当前主要风险

- 雷达当前主要是前向距离信息；如果最终任务要求判断左右两条路线哪边有障碍，需要补充左右侧障碍感知或明确分叉处判定策略。
- 避障动作是定时开环，换电池、电机、地面摩擦或负载后必须重新调参。
- LoRa、雷达、巡线 UART 的实机波特率和接线需要用板级测试逐项确认。
- `TDPS/simulator/` 是从原 `TDPS-Simulator` 整理迁入；当前 `line_follow_cli.sh`/runner 默认路径仍可能引用旧 `TDPS-Simulator/`，必要时手动用当前 `simulator/` 路径构建运行。
- 默认 debug profile 使用低速保守巡线参数并关闭岔路识别；只有明确切换 competition profile 时才启用比赛参数。

## 下一步建议

1. 用 `TDPS/tests/board/tdps_board_test/README.md` 的顺序完成供电、SWD、GPIO、编码器、电机、雷达、巡线和 LoRa 单项测试。
2. 确认 8 路巡线传感器实际输入模式和极性，记录到 `TDPS/docs/tuning.md` 或测试记录。
3. 在实车上先架空确认 `Profile debug` 打印和电机方向，再低速测试直线、轻微偏离回正和 U 型弯。
4. 明确左右分叉路线的障碍检测方案，避免只用前向雷达做无法区分左右侧的决策。
5. 将每次上板测试的日期、场地、电池电压、debug/competition 参数和现象写入实验记录。
