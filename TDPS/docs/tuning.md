# 整车测试调参说明

> **2026-06-01 架构重构**：巡线控制已从 62 参数 PID 系统重构为 **6 参数简化 PD + 曲率前馈(kff)** 架构。
> 核心公式：`correction = kp*error + kd*derivative + kff*derivative*speed`，速度采用连续函数 `speed = base_speed - (base_speed - min_speed) * |error| / 1750`。
> 开关：`TDPS_SIMPLE_CONTROL`（`lf_app.c:1`，1=简化, 0=原始）。简化模式下死区/软区/积分/速度缩放/变化率限幅/二重滤波均关闭。
> **可调参数从 62 个缩减为 6 个**：`kp`, `kd`, `kff`, `base_speed`, `min_speed`, `max_correction`。

整车测试时，手动调整的参数集中在 `firmware/Src/lf_config.c` 的 `g_lf_config`。不要分散修改算法文件。每次只改 1-2 个参数，记录"场地/电池电压/参数/现象/结果"。

## 推荐调试顺序

1. 传感器极性、通道顺序和权重方向
2. 6 核心参数：`base_speed` → `kp` → `kd` → `kff` → `min_speed` → `max_correction`
3. 直线脏点/褶皱/反光抑制 `straight_noise_*`
4. 长前探补偿 `lead_advance_ticks`
5. 补偿后的转向保持 `lead_turn_delta` / `lead_turn_hold_ticks`
6. 特殊线型触发阈值 `lead_event_*`
7. 丢线保持与恢复
8. 雷达 WARN/BLOCK 阈值
9. 避障动作时间和速度
10. LoRa 检查点联调

## 1. 主循环与启动参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `control_period_ms` | `10` | 巡线控制周期。越小响应越快，但 CPU/串口调试压力更大。 | 默认保持 10ms，不建议现场改。若电机响应明显慢，优先调 PID 和速度。 |
| `auto_start_delay_ms` | `0`（debug profile） | 上电后未按启动键时自动开始标定的等待时间。 | debug profile 用于快速实车低速验证；需要更多摆车/放车时间就加大。 |

## 2. 标定参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `calibration_duration_ms` | `500`（debug profile） | 标定总时长，用于采集每路传感器黑白 min/max。 | debug profile 走 Yahboom UART + 快速标定；标定后经常误判黑白时加长。 |
| `calibration_switch_interval_ms` | `300` | 标定时左右摆动切换周期。 | 摆动覆盖不到黑线两侧时增大；摆动太慢时减小。 |
| `calibration_spin_speed` | `180` | 标定时原地摆动速度。 | 传感器扫不过黑白边界就增大；车体抖动或滑出线就减小。 |

## 3. 传感器参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `sensor_input_mode` | `LF_SENSOR_INPUT_UART_PROTOCOL`（debug profile） | 8 路传感器输入模式。默认 debug profile 走 Yahboom 8-LP UART；默认配置仍可用于 ADC/GPIO。 | 实机接 UART 就保持 debug profile；如果 8-LP 用 IO 模式，改为 `LF_SENSOR_INPUT_DIGITAL_GPIO`。 |
| `sensor_invert_polarity` | `true`（debug profile） | 归一化后极性翻转。 | 黑线原始值比白底低或位置方向反时检查此项；若 ADC/GPIO 与 debug profile 不同，先校验极性再调 PID。 |
| `sensor_digital_threshold` | `2048` | 数字/GPIO 判高低的阈值。 | 仅数字模式相关。临界抖动时远离黑白中间值。 |
| `sensor_digital_active_high` | `false` | 数字模式下"在线"是否为高电平。 | 黑线时 IO 输出高则设 `true`，黑线时输出低则保持 `false`。 |
| `sensor_use_dynamic_calibration` | `false`（debug profile） | 是否启用上电动态标定。 | ADC 模式建议开启；当前 Yahboom UART debug profile 关闭动态标定并使用快速标定。 |
| `sensor_filter_alpha` | `0.35`（debug profile） | 低通滤波系数。越大越灵敏，越小越平滑。 | 当前以 `0.35` 为低速实车基准；若调大，必须单独验证直线不蛇形。 |
| `line_detect_min_sum` | `780` | 判定"检测到线"的总强度阈值。 | 经常误丢线就降低；白底也被认为在线就升高。先确认极性再调。 |
| `line_detect_min_peak` | `260` | 单通道在线峰值阈值。 | 偶发脏点被认为在线就升高；真实线峰值偏低才降低。 |
| `line_detect_min_contrast` | `80`（debug profile） | 峰值与背景差值阈值。 | 当前为低速上板基准；过高会在宽线/低对比处误进入低速或丢线。 |
| `edge_hint_threshold` | `120` | 左右半区强度差超过该值时更新丢线恢复方向。 | 丢线后总往错边找线就检查权重/极性，再调此值；方向变化太敏感就升高。 |
| `sensor_edge_noise_reject_enable` | `true`（debug profile） | 计算位置/方向前，抑制 0/7 通道孤立假阳性。 | 直线地面灰尘、褶皱阴影让 0 或 7 单独亮时保持开启；若真实极边线被忽略，再检查邻居阈值。 |
| `sensor_edge_noise_neighbor_threshold` | `220`（debug profile） | 0/7 假点抑制所需的相邻通道阈值。 | 值越高越容易把孤立边缘点剔除；若边缘真实线被剔除就降低。 |
| `sensor_weights` | `{1750,1250,750,250,-250,-750,-1250,-1750}`（debug profile） | 8 路横向位置权重；当前 Yahboom UART 实车通道顺序与常规"左负右正"表述相反，以 debug profile 为准。 | 不要随意改回 `{-1750..1750}`：曾导致直线不稳、箭头岔路摆头走错线、U 型弯入口反向左转。 |

## 4. 巡线 PD + kff 核心参数（简化架构）

当前架构为 **6 参数 PD + 曲率前馈**。各参数独立作用、互不耦合：

| 参数 | 当前值 | 作用 | 调试方法 |
|---|---:|---|---|
| `base_speed` | `280`（debug profile） | 直线全速。连续速度函数的最高点。 | 粗扫确认 280 全面优于 400。直线不稳先调 kp/kd，不要降速度。 |
| `min_speed` | `60`（debug profile） | 弯道最低速。连续速度函数的最低点。 | 弯道冲出就降低；过弯太慢就提高。 |
| `kp` | `0.25`（kff 精扫最佳） | 比例增益，当前位置偏差修正。 | 两轮扫描（粗扫 36 组 + kff 精扫 27 组）确定。直线贴线不够就加，摆头就减。 |
| `kd` | `1.20`（kff 精扫最佳） | 微分阻尼 + 传感器 22cm 预瞄。 | 利用长轴距天然预瞄优势。弯道抖动就加，响应迟钝就减。 |
| `kff` | `0.0008`（kff 精扫最佳） | 曲率前馈增益。不等偏差出现就预打方向。 | 扫描确认降低 19% 修正变化量。入弯仍偏晚可微增到 0.001，直线蛇形则减。 |
| `max_correction` | `300`（粗扫最佳） | 差速硬上限。 | 300 明显优于 180。弯道转向不足可加大，直线摆头可减小。 |
| `ki` | `0.0` | 积分增益。简化模式关闭。 | 巡线无静差，积分在弯道累积导致过冲画龙。 |
| `control_error_deadband` | `0`（简化模式） | 死区。简化模式关闭。 | 死区让微偏不修正，直线贴线变差。 |
| `control_error_soft_zone` | `0`（简化模式） | 软区。简化模式关闭。 | 二次压缩让中等偏差响应变钝。 |
| `max_output_delta_per_tick` | `0`（简化模式） | 变化率限幅。简化模式关闭。 | 限制修正响应速度，与 max_correction 竞争。 |
| `derivative_filter_alpha` | `0.0`（简化模式） | 微分二重滤波。简化模式关闭。 | 传感器前端已有 IIR，再滤波等于砍掉预瞄。 |

速度公式：`speed = base_speed - (base_speed - min_speed) * |error| / 1750`，加 IIR 平滑（alpha=0.4）。差速合成：`left = speed + correction, right = speed - correction`。

## 5. 直道高速、弯前减速与抗干扰参数

> 简化模式下 `straight_boost_enable` 和 `curve_prepare_enable` 均已关闭，由连续速度函数替代。以下参数仅在切回原始架构时有效。

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `straight_boost_enable` | `false`（简化模式关闭） | 直道高速开关。 | 连续速度函数替代；关闭避免状态机误判。 |
| `curve_prepare_enable` | `false`（简化模式关闭） | 弯前减速开关。 | 连续速度函数替代；关闭避免计数器互相清零。 |
| `straight_noise_reject_enable` | `true`（debug profile） | 是否把弱宽线/脏点/褶皱/反光识别为直线噪声。 | 用于解决直线突然变慢；若真弯道被当噪声，收紧阈值。 |
| `straight_noise_confirm_ticks` | `2`（debug profile） | 连续多少帧确认直线噪声。 | 直线仍常误减速可减小到 1；真弯被误判噪声就增大。 |
| `straight_noise_active_count_threshold` | `5`（debug profile） | 触发直线噪声候选的最小在线通道数。 | 脏点/反光漏识别可降低；普通弯道被误判可提高。 |
| `straight_noise_max_sum` | `1800`（debug profile） | 直线噪声允许的最大总强度。 | 与 `lead_event_min_sum=2200` 拉开距离；强宽黑/路口不应归为噪声。 |
| `straight_noise_max_position_error` | `250`（debug profile） | 直线噪声允许的最大中心偏差。 | 直线反光偏差较大可放宽；弯道误判为噪声就收紧。 |
| `straight_noise_max_position_delta` | `120`（debug profile） | 相对上一可信位置的最大跳变。 | 地图褶皱导致小跳变可放宽；大跳变应交给干扰/恢复逻辑。 |
| `interference_active_count_threshold` | `5`（debug profile） | 疑似宽黑/箭头干扰的激活传感器数量。 | 用于压制箭头型岔路宽黑干扰；若真实岔路被压制再提高。 |
| `interference_position_jump_threshold` | `320`（debug profile） | 干扰帧相对上一可信位置的跳变阈值。 | 用于减少箭头岔路和 U 型入口突然大摆头。 |
| `direction_update_confidence_min` | `0.50`（debug profile） | 更新丢线恢复方向所需最低置信度。 | 避免低置信干扰帧刷新可信方向。 |

## 6. 长前探车身与特殊线型参数

当前三轮差速车的传感器阵列中心到左右驱动轮轴线中点约 22 cm，左右驱动轮中心距约 16 cm。T 字路口、直角转弯、圆形入口、U 型顶点和多线交点应以左右驱动轮轴线中点作为转向参考点；前置传感器刚看到特殊线型时，不要立即强制转弯。

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `lead_compensation_enable` | `false`（debug profile） | 是否启用特殊线型前探补偿。 | 当前关闭，先验证简化 PD+kff 基础巡线。 |
| `lead_event_active_count_threshold` | `6`（debug profile） | 判定特殊线型所需在线通道数。 | 路口漏触发可降到 5；直线误触发就升高。 |
| `lead_event_min_sum` | `2200`（debug profile） | 特殊线型最小总强度。 | 路口/U 型顶点漏触发可降低；脏点/反光误触发就提高。 |
| `lead_event_center_error_threshold` | `350`（debug profile） | 宽线/交点中心帧允许的中心偏差。 | U 型顶点全通道在线但 `position≈0` 时靠它触发。误触发则收紧。 |
| `lead_event_entry_error_threshold` | `650`（debug profile） | 记录入弯/入口方向的位置偏差阈值。 | 必须显著大于 `lead_event_center_error_threshold`，避免阈值重叠导致宽噪声误触发前探。 |
| `lead_event_confirm_ticks` | `2`（debug profile） | 连续多少帧确认特殊线型。 | 路口漏触发可减到 1；直线误触发前探就增大。 |
| `lead_advance_ticks` | `18`（debug profile） | 检测到特殊线型后继续低速直行的 tick 数。 | 最优先调。U 型过早转弯就增大；冲过交点/顶点才转就减小。 |
| `lead_advance_speed` | `65`（debug profile） | 前探补偿阶段左右轮同速。 | 调距离优先改 `lead_advance_ticks`，不要先大幅改速度。 |
| `lead_turn_hold_ticks` | `16`（debug profile） | 补偿后保持差速转向的 tick 数。 | 转向保持不够才增大；过度转向就减小。 |
| `lead_turn_speed` | `55`（debug profile） | 转向保持阶段前进速度。 | 当前低速保守；过快会冲出，过慢可能原地抖。 |
| `lead_turn_delta` | `115`（debug profile） | 转向保持阶段左右轮速度差。 | 补偿后仍转不过去再增大；转向过猛就减小。 |
| `lead_entry_memory_ticks` | `40`（debug profile） | 入口方向记忆保留时间。 | 方向丢得太快就增大；过旧方向导致误转就减小。 |

调参顺序固定为：先 `lead_advance_ticks`，再 `lead_turn_delta`，再 `lead_turn_hold_ticks`，最后才调 `lead_event_*`。如果没有明确入口方向，代码只做保守前探，不随机强制转向。`lead_event_entry_error_threshold` 不能小于或接近 `lead_event_center_error_threshold`。

## 7. 丢线恢复参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `line_lost_grace_ticks` | `4`（默认值） | 短暂丢线后保持最后可信方向的周期数。 | U 型顶点短暂丢线就加；保持方向过久偏离就减。 |
| `line_hold_speed` | `0`（debug profile） | grace 阶段低速前进速度。 | debug baseline 默认不做保持前进；找线太慢再小步加。 |
| `line_hold_turn_speed` | `0`（debug profile） | grace 阶段按可信方向转向的速度差。 | debug baseline 默认不做保持转向；U 型弯转不过去再小步加。 |
| `recover_sweep_start_speed` | `180`（默认值） | 进入扫线时的初始转向速度。 | 找线太慢就加；扫过线就减。 |
| `recover_sweep_max_speed` | `280`（默认值） | 恢复扫线最大转向速度。 | 超时前仍找不到线就加；原地转圈明显就减。 |
| `recover_timeout_ms` | `3000`（debug profile） | 丢线恢复最长时间，超时停车。 | 短暂离线即可找回但停车太早就加；失控转圈太久就减。 |

## 8. 雷达判定参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `radar_enable` | `false`（debug profile） | 是否启用雷达解析和避障状态。 | 单独调巡线时保持关闭；整车联调再开启。 |
| `radar_uart_baudrate` | `115200` | LD2410S 串口波特率记录值。实际 UART 初始化仍需与 CubeMX/板级配置一致。 | 如果完全无雷达帧，先确认 CubeMX USART 波特率、接线和中断。 |
| `radar_trigger_distance_mm` | `450` | 进入 `BLOCK` 的距离阈值。 | 车来不及避障就加大；离障碍很远就绕行就减小。 |
| `radar_release_distance_mm` | `650` | 从 `BLOCK/WARN` 释放到 `CLEAR` 的距离阈值。应大于 trigger，形成滞回。 | 状态反复跳变就加大与 trigger 的差值；解除太慢就减小。 |
| `radar_debounce_frames` | `3` | 连续多少帧确认状态变化。 | 误报多就加；反应慢就减。 |
| `radar_frame_timeout_ms` | `300` | 超过该时间没有有效帧则清除雷达状态。 | 雷达帧率低导致状态闪断就加；断线后旧状态保持太久就减。 |

## 9. 避障参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `obstacle_warn_speed` | `220` | `WARN` 状态下的最高巡线速度。 | 接近障碍仍太快就减；减速过早影响通行就加。 |
| `obstacle_avoid_enable` | `false`（debug profile） | 是否启用 `BLOCK` 后自动绕障。 | 只验证巡线时保持关闭；整车任务设 `true`。 |
| `obstacle_preferred_side` | `0` | 绕障方向：`-1` 左，`+1` 右，`0` 自动。 | 地图已知空侧时固定方向；随机障碍先用 0，失败会反向重试。 |
| `obstacle_max_attempts` | `2` | 避障失败最多尝试次数。 | 一次失败后需要反向重试就保持 2；场地狭窄避免乱跑可设 1。 |
| `obstacle_confirm_ms` | `100` | BLOCK 后停车确认时间。 | 雷达误报多就加；反应太慢就减。 |
| `obstacle_stop_ms` | `120` | 开始绕障前停车稳定时间。 | 车体惯性大、入弯前滑动就加；动作拖沓就减。 |
| `obstacle_turn_out_ms` | `360` | 从原线路转出的持续时间。 | 绕不过障碍、转出角度小就加；转太大找不回线就减。 |
| `obstacle_bypass_ms` | `900` | 绕过障碍的前进弧线持续时间。 | 还没越过障碍就转回就加；绕得太远离线就减。 |
| `obstacle_turn_in_ms` | `360` | 转回主线路方向的持续时间。 | 回线角度不够就加；横穿过线或反向过头就减。 |
| `obstacle_reacquire_timeout_ms` | `1600` | 避障后找线最长时间。 | 已接近线但停车太早就加；找不到线还转太久就减。 |
| `obstacle_turn_speed` | `240` | 避障转出/转入的转向速度。 | 转不动或角度不足就加；打滑或转过头就减。 |
| `obstacle_bypass_inner_speed` | `160` | 绕障弧线内侧轮速度。 | 绕行半径太大就降低内侧或提高外侧；半径太小就提高内侧。 |
| `obstacle_bypass_outer_speed` | `320` | 绕障弧线外侧轮速度。 | 绕不过障碍就提高；速度过快撞障或离线就降低。 |
| `obstacle_emergency_distance_mm` | `280` | 避障过程中仍过近时立即停止并重试。 | 仍会撞障就加大；频繁急停导致无法绕过就减小。 |

## 10. 实车现象到参数的快速映射

| 现象 | 优先检查/调整 |
|---|---|
| 上电标定后一直认为丢线 | `sensor_invert_polarity`、`sensor_digital_active_high`、`line_detect_min_sum` |
| 直道能走但左右晃动很厉害 | `kp` 过大→降低；`kd` 不足→增加；确认 `control_error_deadband=0` |
| 直道微偏不修、压线跑偏 | `kp` 不足→增加；确认 `control_error_soft_zone=0` |
| 弯道入弯偏晚、冲出 | `kff` 不足→微增（如 0.0008→0.001）；`min_speed` 偏高→降低 |
| 弯道内抖动、画龙 | `kd` 不足→增加；`kff` 过大→降低 |
| 直线遇脏点/褶皱/反光突然变慢 | 优先调 `straight_noise_*`；不要动 kp/kd/base_speed |
| U 型弯入口冲出 | 先调 `lead_advance_ticks` 让驱动轮轴线中点到达顶点附近，再调 `lead_turn_delta` |
| 丢线后找错方向 | 先查传感器极性和 `sensor_weights`，再调 `edge_hint_threshold` |
| 雷达无数据 | 查 UART 接线、波特率、IRQ、`radar_enable` |
| 雷达误触发 | 加 `radar_debounce_frames`，减小 `radar_trigger_distance_mm` |
| 快撞上才避障 | 加大 `radar_trigger_distance_mm`，降低 `base_speed`/`obstacle_warn_speed` |
| 绕障转出不够 | 加 `obstacle_turn_out_ms` 或 `obstacle_turn_speed` |
| 绕障没越过障碍 | 加 `obstacle_bypass_ms` 或 `obstacle_bypass_outer_speed` |
| 绕障后找不到线 | 调 `obstacle_turn_in_ms`，加 `obstacle_reacquire_timeout_ms`，降低绕障速度 |

## 11. 参数版本台账

### 当前版本（2026-06-01，简化 PD+kff 架构）

以 `LF_Config_ApplyDebugProfile()` 为上板基准：

| 参数组 | 当前值 |
|---|---|
| 传感器 | UART、`sensor_invert_polarity=true`、`sensor_filter_alpha=0.35`、`line_detect_min_sum=780`、`line_detect_min_peak=260`、`line_detect_min_contrast=80` |
| 权重 | `{1750,1250,750,250,-250,-750,-1250,-1750}` |
| 核心 6 参数 | `base_speed=280`、`min_speed=60`、`kp=0.25`、`kd=1.20`、`kff=0.0008`、`max_correction=300` |
| 关闭项 | `ki=0`、`deadband=0`、`soft_zone=0`、`max_output_delta_per_tick=0`、`derivative_filter_alpha=0.0`、`integral_limit=0` |
| 策略开关 | `straight_boost_enable=false`、`curve_prepare_enable=false`、`lead_compensation_enable=false`、`fork_enable=false`、`obstacle_avoid_enable=false` |
| 直线噪声 | `straight_noise_reject_enable=true`、`straight_noise_confirm_ticks=2`、`straight_noise_active_count_threshold=5`、`straight_noise_max_sum=1800` |

### 模拟器参数扫描结果

| 轮次 | 扫描范围 | 结论 |
|---|---|---|
| 粗扫 36 组 | kp=0.15~0.40, kd=0.40~1.20, base=280/400, max_correction=180/300 | base_speed=280 全面优于 400；max_correction=300 明显优于 180；kd=1.0~1.2 优于 0.4~0.8 |
| kff 精扫 27 组 | kp=0.15~0.25, kd=0.80~1.20, kff=0~0.0012 | kp=0.25, kd=1.20, kff=0.0008 最优；kff=0.0008 降低 19% 修正变化量 vs kff=0 |

### 历史版本（重构前，仅供参考）

| 参数组 | 旧值 |
|---|---|
| PID/速度 | `base_speed=150`、`sharp_turn_speed=35`、`kp=0.09`、`kd=0.22`、`ki=0`、`max_correction=250` |
| 死区/软区 | `control_error_deadband=80`、`control_error_soft_zone=240` |
| 输出限制 | `derivative_filter_alpha=0.60`、`max_output_delta_per_tick=28` |
| 策略开关 | `straight_boost_enable=false`、`curve_prepare_enable=true`、`line_stability_enable=true`、`lead_compensation_enable=true` |

重构前已试过但不好用的参数组合：

| 版本/参数 | 现象 | 结论 |
|---|---|---|
| `control_error_deadband=140`、`soft_zone=420` | 直线很稳但偏离后几乎不修正，弯道入口直走。 | 抗摆参数过强吃掉中等偏差；简化架构直接去掉死区/软区。 |
| `base_speed=100`、`kp=0.08`、`max_correction=50` | 直线保守但 U 型弯转向不足。 | max_correction 太小限制差速能力。 |
| Kp 速度缩放（低速弯 Kp 被压到 0.18x） | P 项形同虚设，全部依赖 D 项和硬限幅暴力转向。 | 去掉速度缩放，kp 恒定。 |
| 6 级速度优先级链 | 改一个参数影响 4 个系统，优先级遮蔽。 | 连续速度函数替代。 |
| 仅提高 kp 或 max_correction 试图救 U 型弯 | 改善打角但路口/顶点仍因传感器提前 22 cm 过早转向。 | 根因是长前探几何，不是纯 PID。 |

## 12. 仿真参数选择依据

Simulator 默认 override 仍用于离线 normal/stress 回归；上板验证默认以 debug profile 为准。

已验证命令示例：

```bash
bash simulator/scripts/line_follow_cli.sh quick 15 0.01 0.12 \
  simulator/artifacts/line_follow_v1/reports/single_run/quick_final.json \
  20260319 simulator/sim_tests/line_follow_v1/config/scenarios_default.csv normal

bash simulator/scripts/line_follow_cli.sh full-course \
  simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  simulator/artifacts/line_follow_v1/reports/single_run/full_course_final_stress.json \
  20260319 stress
```

## 13. 每次测试建议记录

```text
日期/场地：
电池电压：
传感器模式和极性：
base_speed/kp/kd/kff/min_speed/max_correction：
lead_advance_ticks/lead_turn_delta/lead_turn_hold_ticks：
straight_noise_*：
雷达 trigger/release：
避障 turn_out/bypass/turn_in/reacquire：
现象：
下一步：
```
