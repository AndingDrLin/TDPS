# 当前项目进度

## 已实现内容

- 巡线：8 路灰度传感器处理、标定、归一化、滤波、**6 参数简化 PD + 曲率前馈(kff)**、连续速度函数、直线噪声抑制、长前探特殊线型补偿、箭头/宽黑干扰保护、可信方向丢线恢复和岔路识别。
- **控制架构（2026-06-01 重构）**：从 62 参数 PID 系统重构为 6 参数 PD+kff。`TDPS_SIMPLE_CONTROL` 条件编译开关（`lf_app.c:1`）。核心参数经两轮模拟器扫描确定：`kp=0.25, kd=1.20, kff=0.0008, base_speed=280, min_speed=60, max_correction=300`。
- 雷达：HLK-LD2410S 串口帧解析、`CLEAR/WARN/BLOCK` 状态输出、WARN 减速、BLOCK 开环绕障与重新寻线。
- 雷达：HLK-LD2410S 串口帧解析、`CLEAR/WARN/BLOCK` 状态输出、WARN 减速、BLOCK 开环绕障与重新寻线。
- 无线：EWM22A LoRa 异步队列、超时重试、可选 ACK、检查点事件入口 `LF_App_NotifyCheckpoint(checkpoint_id)`。
- 测试：PC stub、CMake/gcc 离线测试、Simulator 回归、板级测试程序和独立雷达参考程序。

## 里程碑状态

来源：`TDPS/docs/basic_design.md` 和现有调研记录。

| 里程碑 | 内容 | 当前状态 |
|---|---|---|
| M1 | 底盘与外设联通 | 已完成 / 待用最新实物状态确认 |
| M2 | 基础巡线闭环 | 已完成（2026-06-01 重构为简化 PD+kff 架构） |
| M3 | 丢线恢复稳定 | on track |
| M4 | LoRa 窗口发送接入 | on track |
| M5 | 雷达决策接入 | 已有实现，待实机确认 |
| M6 | 三任务统一联调 | 待全车实测 |

## 仿真结果记录

最新参数扫描结果（2026-06-01）：

- **粗扫 36 组**（kp/kd/base/max_correction）：`base_speed=280` 全面优于 400；`max_correction=300` 明显优于 180；`kd=1.0~1.2` 优于 0.4~0.8。
- **kff 精扫 27 组**：`kp=0.25, kd=1.20, kff=0.0008` 最优；kff=0.0008 降低 19% 修正变化量 vs kff=0。
- 历史 quick normal：16 个场景 High，`overall_score=95.41`。

## 当前主要风险

- 雷达当前主要是前向距离信息；如果最终任务要求判断左右两条路线哪边有障碍，需要补充左右侧障碍感知或明确分叉处判定策略。
- 避障动作是定时开环，换电池、电机、地面摩擦或负载后必须重新调参。
- LoRa、雷达、巡线 UART 的实机波特率和接线需要用板级测试逐项确认。
- `TDPS/simulator/` 路径引用已全部修正（2026-06-01）；几何参数已与实车对齐（传感器前距 22cm、轮距 16cm）。
- 默认 debug profile 使用低速保守巡线参数并关闭岔路识别；只有明确切换 competition profile 时才启用比赛参数。
- 当前实车几何为传感器阵列中心到左右驱动轮轴线中点约 22 cm、左右轮距约 16 cm；`lead_advance_ticks` 仍是时间近似，需要在实车上按 T 字、直角、圆形入口和 U 型顶点重新标定。

## 已知问题

- 赛道固定路线脚本：直角左弯→T口右弯→直角左弯→两个十字直行→直角左弯。
  - 硬事件检测基于 D帧（`$Dx1:x,...,x8:x#`），D帧极性：`0`=黑线=LED亮，`1`=白底=LED灭。
  - `route_lane_on_now(index)` 返回 `digital[index] == 0U`（LED亮时返回true）。
  - T口/十字：8路全亮 → `frame_looks_like_all_on_cross()`。
  - 左直角：外侧第8通道灭，1~6通道亮；或第1通道灭，3~8通道亮。
  - 路线阶段机不走冷却，重复触发由"离开后重新武装"逻辑防抖。
  - **当前问题**：直角弯处车停下、电机抽动后丢线，没有正确执行原地旋转；T口有右转趋势但也没有正确执行固定旋转。疑似原地旋转指令输出或电机差速限制问题。
- 当前实车几何为传感器阵列中心到左右驱动轮轴线中点约 22 cm、左右轮距约 16 cm；`lead_advance_ticks` 仍是时间近似，需要在实车上按 T 字、直角、圆形入口和 U 型顶点重新标定。

## 下一步建议

1. 修复原地旋转执行问题：检查 `fixed_turn_spin_speed=200` 与 `max_motor_delta=420`/`motor_deadband=120` 的配合，确认 `set_opposite_spin_command()` 能正确输出两轮反向命令。
2. 用 Keil Watch 确认：`s_stats.digital_frame_count` 是否持续增长、`uart_digital[0..7]` 在直角/T口灯型变化时的值。
3. 按 `docs/tuning.md` 的参数台账记录每次实车测试，重点关注 `base_speed/kp/kd/kff` 和 `lead_advance_ticks/lead_turn_delta`。
4. 明确左右分叉路线的障碍检测方案，避免只用前向雷达做无法区分左右侧的决策。
5. 将每次上板测试的日期、场地、电池电压、debug 参数和现象写入实验记录。
