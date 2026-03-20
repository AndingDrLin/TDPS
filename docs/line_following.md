# 巡线子系统设计（line_follow_v1）

本文档描述当前仓库中巡线系统的实现方案。重点是“为什么这样做”和“代码边界在哪里”。

## 1. 设计目标

- 在常见扰动下保持持续压线
- 丢线后可恢复，不进入无限振荡
- 关键参数可集中调节
- 可通过离线场景批量回归

## 2. 关键实现

### 2.1 传感器处理

- 传感器数量：4 路
- 每路输入来自 ADC DMA 缓冲
- 先做归一化，再做低通滤波
- 采用加权位置计算偏差（左负右正）

相关文件：

- `Src/lf_sensor.c`
- `Inc/lf_sensor.h`
- `Src/lf_config.c`（权重和阈值）

### 2.2 控制器

- 控制器类型：PID
- 输入：线位置偏差
- 输出：修正量 `correction`
- 底盘命令：`left = base - correction`，`right = base + correction`
- 对输出和电机命令做限幅，避免打满

相关文件：

- `Src/lf_control.c`
- `Src/lf_chassis.c`

### 2.3 状态机

状态定义：`WAIT_START -> CALIBRATING -> RUNNING -> RECOVERING -> STOPPED`

核心行为：

- `CALIBRATING`：左右摆动采样，提高 min/max 覆盖度
- `RUNNING`：按固定周期读传感器 + 更新 PID
- `RECOVERING`：按“最后看到线的方向”旋转搜索
- 超时进入 `STOPPED`，避免失控持续运行

相关文件：

- `Src/lf_app.c`
- `Inc/lf_app.h`

## 3. 参数策略

参数集中在 `Src/lf_config.c` 的 `g_lf_config`。

建议顺序：

1. `base_speed`
2. `kp`
3. `kd`
4. `line_detect_min_sum`
5. `recover_turn_speed` / `recover_timeout_ms`

调参要求：

- 一次只改少量参数
- 每次改动记录场景、参数、结果
- 调参结论以离线报告为准，不靠主观印象

## 4. 离线回归方法

默认场景配置：`TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_default.csv`

覆盖类型：

- 基线场景（circle/figure8/patio）
- 初始偏置（left/right offset）
- 传感器噪声
- 电机不一致
- 组合压力场景

门禁规则由 harness 固化：

- quick：`82 / 0.94 / 0.35 / min_scenario_score=70`
- stability：`80 / 93% / 0.40s`

## 5. 扩展接口

`lf_future_hooks` 提供弱符号空实现，用于后续接入：

- LoRa 窗口触发
- 雷达窗口触发
- 标定开始/结束事件
- 丢线/复线事件

约束：扩展逻辑不得直接破坏 `LF_App_RunStep` 的周期语义。

## 6. 已知限制

- 当前只覆盖巡线主链路，未包含 LoRa/雷达完整业务闭环
- 参数默认值针对当前离线场景调过，换硬件后需要重新标定
- 4 路传感器方案在复杂路口下识别能力有限，后续可扩展传感器阵列与状态判据
