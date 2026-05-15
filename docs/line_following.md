# 巡线子系统设计（line_follow_v1）

本文档描述当前仓库中巡线系统的实现方案。重点是“为什么这样做”和“代码边界在哪里”。

## 1. 设计目标

- 在常见扰动下保持持续压线
- 丢线后可恢复，不进入无限振荡
- 关键参数可集中调节
- 可通过离线场景批量回归

## 2. 关键实现

### 2.1 传感器处理

- 传感器数量：8 路（默认）
- 每路输入来自 ADC DMA 缓冲
- 先做归一化，再做低通滤波
- 采用加权位置计算偏差（左负右正）
- 输出辅助信号：`peak_value`、`line_confidence`、`edge_hint`

与 Yahboom 8-LP 的接口匹配策略：

- GPIO 模式：通过 `LF_SENSOR_INPUT_DIGITAL_GPIO` 读取 8 路高低电平。
- ADC 模式：保留原 `LF_SENSOR_INPUT_ANALOG_ADC` 路径。
- UART/I2C 模式：已预留配置枚举，协议读取后续扩展。
- 极性通过配置解决：
	- `sensor_digital_active_high`（数字模式）
	- `sensor_invert_polarity`（模拟模式）

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

状态定义：`WAIT_START -> CALIBRATING -> RUNNING -> RECOVERING / AVOID_* -> STOPPED`

核心行为：

- `CALIBRATING`：左右摆动采样，提高 min/max 覆盖度
- `RUNNING`：按固定周期读传感器 + 更新 PID
- `RUNNING`：并行轮询雷达状态（非阻塞），`WARN` 减速，`BLOCK` 进入避障
- `RECOVERING`：按“最后看到线的方向”旋转搜索；若遇到 `BLOCK` 也进入避障
- `AVOID_*`：停车确认、转出、弧线绕行、转回、低速找线；失败后反向重试
- 超时或重试失败进入 `STOPPED`，避免失控持续运行

相关文件：

- `Src/lf_app.c`
- `Inc/lf_app.h`

## 3. 参数策略

参数集中在 `Src/lf_config.c` 的 `g_lf_config`。逐项含义和整车调试方法见 `docs/tuning.md`。

建议顺序：

1. `base_speed`
2. `kp`
3. `kd`
4. `line_detect_min_sum`
5. `recover_turn_speed` / `recover_timeout_ms`
6. `obstacle_turn_out_ms` / `obstacle_bypass_ms` / `obstacle_turn_in_ms`
7. `obstacle_reacquire_timeout_ms` / `obstacle_preferred_side`

8-LP 相关关键参数：

- `sensor_input_mode`
- `sensor_digital_threshold`
- `sensor_digital_active_high`
- `sensor_invert_polarity`
- `sensor_use_dynamic_calibration`

调参要求：

- 一次只改少量参数
- 每次改动记录场景、参数、结果
- 调参结论以离线报告为准，不靠主观印象

## 4. 离线回归方法

默认场景配置：`TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_default.csv`

覆盖类型：

- 基线场景（circle/figure8/patio）
- 初始偏置（left/right offset）
- 传感器噪声、掉线、增益/偏置误差
- 电机不一致、一阶惯性、低速非线性、电池压降
- 传感器安装偏差、线宽变化
- 雷达延迟、漏检、误检、距离抖动
- 组合压力场景

门禁规则由 harness 固化：

- quick：`82 / 0.94 / 0.35 / min_scenario_score=70`
- full-course：完整赛道需按顺序触发检查点、到达终点、无碰撞/越界，并满足进度与丢线门限
- stability：`80 / 93% / 0.40s`

当前默认参数在 `scenarios_full_course.csv` 的 normal/stress 下均通过，5 seed stress 稳定性结果为 High。

## 5. 扩展接口

`lf_future_hooks` 提供弱符号空实现，用于后续接入：

- LoRa 窗口触发
- 雷达窗口触发
- 标定开始/结束事件
- 丢线/复线事件

约束：扩展逻辑不得直接破坏 `LF_App_RunStep` 的周期语义。

## 6. 已知限制

- 雷达解析已支持保留测试帧、LD2410S 最小帧和已知标准帧目标/距离字段，仍需实机确认距离单位、波特率和目标状态语义
- 当前避障只有前向雷达信息，不能直接感知障碍在左侧还是右侧；左右绕行依赖配置、上次线方向和失败反向重试
- 避障动作是定时开环，换电池、电机、地面摩擦或车体负载后必须重新调参
- 参数默认值针对当前离线场景调过，换硬件后需要重新标定
