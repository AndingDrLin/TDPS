# 整车测试调参说明

整车测试时，手动调整的参数集中在 `firmware/Src/lf_config.c` 的 `g_lf_config`。不要分散修改算法文件。每次只改 1-2 个参数，记录“场地/电池电压/参数/现象/结果”。

## 推荐调试顺序

1. 传感器极性、通道顺序和权重方向
2. 基础速度 `base_speed` 与急弯低速 `sharp_turn_speed`
3. PID：先 `kp`，再 `kd`，最后通常不启用 `ki`
4. `max_correction` 与 `max_output_delta_per_tick`
5. 直线脏点/褶皱/反光抑制 `straight_noise_*`
6. 长前探补偿 `lead_advance_ticks`
7. 补偿后的转向保持 `lead_turn_delta` / `lead_turn_hold_ticks`
8. 特殊线型触发阈值 `lead_event_*`
9. 箭头/宽黑干扰保护和岔路确认阈值
10. 丢线保持与恢复
11. 雷达 WARN/BLOCK 阈值
12. 避障动作时间和速度
13. LoRa 检查点联调

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
| `sensor_digital_active_high` | `false` | 数字模式下“在线”是否为高电平。 | 黑线时 IO 输出高则设 `true`，黑线时输出低则保持 `false`。 |
| `sensor_use_dynamic_calibration` | `false`（debug profile） | 是否启用上电动态标定。 | ADC 模式建议开启；当前 Yahboom UART debug profile 关闭动态标定并使用快速标定。 |
| `sensor_filter_alpha` | `0.35`（debug profile） | 低通滤波系数。越大越灵敏，越小越平滑。 | 当前以 `0.35` 为低速实车基准；若调大，必须单独验证直线不蛇形。 |
| `line_detect_min_sum` | `780` | 判定“检测到线”的总强度阈值。 | 经常误丢线就降低；白底也被认为在线就升高。先确认极性再调。 |
| `line_detect_min_peak` | `260` | 单通道在线峰值阈值。 | 偶发脏点被认为在线就升高；真实线峰值偏低才降低。 |
| `line_detect_min_contrast` | `80`（debug profile） | 峰值与背景差值阈值。 | 当前为低速上板基准；过高会在宽线/低对比处误进入低速或丢线。 |
| `edge_hint_threshold` | `120` | 左右半区强度差超过该值时更新丢线恢复方向。 | 丢线后总往错边找线就检查权重/极性，再调此值；方向变化太敏感就升高。 |
| `sensor_edge_noise_reject_enable` | `true`（debug profile） | 计算位置/方向前，抑制 0/7 通道孤立假阳性。 | 直线地面灰尘、褶皱阴影让 0 或 7 单独亮时保持开启；若真实极边线被忽略，再检查邻居阈值。 |
| `sensor_edge_noise_neighbor_threshold` | `220`（debug profile） | 0/7 假点抑制所需的相邻通道阈值。 | 值越高越容易把孤立边缘点剔除；若边缘真实线被剔除就降低。 |
| `sensor_weights` | `{1750,1250,750,250,-250,-750,-1250,-1750}`（debug profile） | 8 路横向位置权重；当前 Yahboom UART 实车通道顺序与常规“左负右正”表述相反，以 debug profile 为准。 | 不要随意改回 `{-1750..1750}`：曾导致直线不稳、箭头岔路摆头走错线、U 型弯入口反向左转。 |

## 4. 巡线 PID 与电机参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `base_speed` | `150`（debug profile） | 正常巡线基础速度。 | 当前直线稳定性较好的基准。`100` 太保守且配合 `max_correction=50` 转向不足；`180` 曾让 U 型弯和干扰处更难控。 |
| `straight_boost_speed` | `320`（默认值，debug 关闭） | 连续稳定直道的高速速度。 | debug profile 默认关闭直道高速；需要提速时先明确切换或重新开启。 |
| `curve_prepare_speed` | `70`（默认值，debug 当前关闭） | 检测到入弯趋势后的提前减速速度。 | debug profile 先关闭以保证直线基线；确认直线稳定后，若要开启，必须同步检查直线脏点/反光不会触发误减速。 |
| `sharp_turn_speed` | `35`（debug profile） | 偏差超过急弯阈值后的低速。 | 急弯仍冲出就降低；急弯太慢就小幅提高。 |
| `kp` | `0.09`（debug profile） | 比例增益，决定偏离黑线后的纠正力度。 | 从 1343 基线 `0.10` 小降，目标是减小直线、箭头岔路和 U 型入口摆头。若转弯明显跟不上，再小步回到 `0.10`。 |
| `kd` | `0.22`（debug profile） | 微分增益，抑制快速摆动。 | 从 1343 基线 `0.18` 小增，用于增加阻尼。若弯道响应变钝，再回退到 `0.20`。 |
| `ki` | `0.0` | 积分增益，修长期偏差。 | 通常保持 0。只有持续偏一侧且机械问题排除后，再小幅增加。 |
| `max_correction` | `250`（debug profile，1343 直线基线） | PID 修正量总限幅。 | 这是 1343f3c 能直走时的基线值；不要擅自改成更小来“保守”，否则可能只剩原地左右找线而不前进。 |
| `control_error_deadband` | `80`（debug profile） | PID 前的位置误差死区，小偏差按 0 处理。 | 用于宽线约等于 3 个通道时减少小幅左右修正；若弯道入口跟线明显迟钝，再小步降低。 |
| `control_error_soft_zone` | `240`（debug profile） | PID 前的位置误差软区，死区外的小误差被压缩。 | 软区必须大于死区；抖动仍明显可小步增大，U 型弯跟不上则小步减小。 |
| `max_motor_cmd` | `300`（debug profile） | 电机命令绝对值上限。 | 保护电机/电池。速度不够可加，但先确认机械和供电。 |
| `motor_deadband` | `0`（debug profile） | 非零命令的最小起转补偿。 | 小命令车不动就加；低速动作太冲就减。 |

## 5. 直道高速、弯前减速与抗干扰参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `straight_boost_enable` | `false`（debug profile） | 是否启用连续稳定直道高速。 | 当前默认关闭；先验证低速直线稳定，再考虑重新开启。 |
| `straight_error_threshold` | `250` | 进入直道高速允许的最大位置偏差。 | 仅开启直道高速后相关；直道不提速可增大，误把小弯当直道就减小。 |
| `straight_delta_threshold` | `120` | 直道高速允许的相邻可信位置变化。 | 仅开启直道高速后相关；传感器噪声导致不提速可增大，摆头明显就减小。 |
| `straight_confidence_min` | `0.55` | 直道高速最低置信度。 | 仅开启直道高速后相关；低对比场地不提速可略降，误提速就提高。 |
| `straight_confirm_ticks` | `8` | 连续多少个控制周期稳定后提速。 | 仅开启直道高速后相关；提速太慢就减小，提速太早就增大。 |
| `curve_prepare_enable` | `true`（debug profile，保留 1343 直线基线） | 是否启用弯前趋势减速。 | 保留 1343f3c 能直走时的基线开关；本轮稳定化不再改它，避免重现原地左右转。 |
| `curve_prepare_error_threshold` | `400` | 入弯趋势的位置偏差阈值。 | U 型弯冲出就降低；直道误减速先检查 `straight_noise_*`，再考虑提高。 |
| `curve_prepare_delta_threshold` | `180`（debug profile） | 入弯趋势的位置变化阈值。 | 本轮从 100 提到 180，避免单帧边缘假点/宽线时间差让直线误进入低速；弯前减速太晚再小步降低。 |
| `curve_prepare_confirm_ticks` | `3`（debug profile） | 连续多少个控制周期确认入弯。 | 本轮从 1 提到 3，给速度模式增加确认；直线速度误降优先保持 3，真实急弯减速慢再回到 2。 |
| `straight_noise_reject_enable` | `true`（debug profile） | 是否把弱宽线/脏点/褶皱/反光识别为直线噪声。 | 当前用于解决直线突然变慢；若真弯道被当噪声，收紧下面几个阈值。 |
| `straight_noise_confirm_ticks` | `2`（debug profile） | 连续多少帧确认直线噪声。 | 直线仍常误减速可减小到 1；真弯被误判噪声就增大。 |
| `straight_noise_active_count_threshold` | `5`（debug profile） | 触发直线噪声候选的最小在线通道数。 | 脏点/反光漏识别可降低；普通弯道被误判可提高。 |
| `straight_noise_max_sum` | `1800`（debug profile） | 直线噪声允许的最大总强度。 | 与 `lead_event_min_sum=2200` 拉开距离；强宽黑/路口不应归为噪声。 |
| `straight_noise_max_position_error` | `250`（debug profile） | 直线噪声允许的最大中心偏差。 | 直线反光偏差较大可放宽；弯道误判为噪声就收紧。 |
| `straight_noise_max_position_delta` | `120`（debug profile） | 相对上一可信位置的最大跳变。 | 地图褶皱导致小跳变可放宽；大跳变应交给干扰/恢复逻辑。 |
| `interference_active_count_threshold` | `5`（debug profile） | 疑似宽黑/箭头干扰的激活传感器数量。 | 本轮从 6 降到 5，用于提前压制箭头型岔路宽黑干扰；若真实岔路被压制再提高。 |
| `interference_position_jump_threshold` | `320`（debug profile） | 干扰帧相对上一可信位置的跳变阈值。 | 本轮从 450 降到 320，用于减少箭头岔路和 U 型入口突然大摆头；若正常弯道被误判干扰再提高。 |
| `direction_update_confidence_min` | `0.50`（debug profile） | 更新丢线恢复方向所需最低置信度。 | 本轮从 0.45 提到 0.50，避免低置信干扰帧刷新可信方向。 |

## 6. 长前探车身与特殊线型参数

当前三轮差速车的传感器阵列中心到左右驱动轮轴线中点约 22 cm，左右驱动轮中心距约 16 cm。T 字路口、直角转弯、圆形入口、U 型顶点和多线交点应以左右驱动轮轴线中点作为转向参考点；前置传感器刚看到特殊线型时，不要立即强制转弯。

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `lead_compensation_enable` | `true`（debug profile） | 是否启用特殊线型前探补偿。 | 当前在 `56ecd1b` 直线稳定基线上保守开启，用于 U 型顶点先低速前探再轻转；若直线明显退化，优先回退到该基线对照。 |
| `lead_event_active_count_threshold` | `6`（debug profile） | 判定特殊线型所需在线通道数。 | 路口漏触发可降到 5；直线误触发就升高。 |
| `lead_event_min_sum` | `2200`（debug profile） | 特殊线型最小总强度。 | 路口/U 型顶点漏触发可降低；脏点/反光误触发就提高。 |
| `lead_event_center_error_threshold` | `350`（debug profile） | 宽线/交点中心帧允许的中心偏差。 | U 型顶点全通道在线但 `position≈0` 时靠它触发。误触发则收紧。 |
| `lead_event_entry_error_threshold` | `650`（debug profile） | 记录入弯/入口方向的位置偏差阈值。 | 必须显著大于 `lead_event_center_error_threshold`，避免阈值重叠导致几乎所有宽线/噪声都触发前探。 |
| `lead_event_confirm_ticks` | `2`（debug profile） | 连续多少帧确认特殊线型。 | 路口漏触发可减到 1；直线误触发前探就增大。 |
| `lead_advance_ticks` | `14`（debug profile） | 检测到特殊线型后继续低速直行的 tick 数。 | 最优先调。U 型过早转弯就增大；冲过交点/顶点才转就减小。 |
| `lead_advance_speed` | `60`（debug profile） | 前探补偿阶段左右轮同速。 | 当前用于让驱动轮轴线接近 U 型顶点；调距离优先改 `lead_advance_ticks`，不要先大幅改速度。 |
| `lead_turn_hold_ticks` | `7`（debug profile） | 补偿后保持差速转向的 tick 数。 | 当前刻意短保持，避免 U 型顶点过度向左；转向保持不够才小步增大。 |
| `lead_turn_speed` | `50`（debug profile） | 转向保持阶段前进速度。 | 当前低速保守；过快会冲出，过慢可能原地抖。 |
| `lead_turn_delta` | `80`（debug profile） | 转向保持阶段左右轮速度差。 | 当前从 115 降低，避免 U 型顶点强制转向过猛；补偿后仍转不过去再小步增大。 |
| `lead_entry_memory_ticks` | `32`（debug profile） | 入口方向记忆保留时间。 | 当前缩短以减少过旧方向导致误转；方向丢得太快再增大。 |

调参顺序固定为：先 `lead_advance_ticks`，再 `lead_turn_delta`，再 `lead_turn_hold_ticks`，最后才调 `lead_event_*`。如果没有明确入口方向，代码只做保守前探，不随机强制转向。`lead_event_entry_error_threshold` 不能小于或接近 `lead_event_center_error_threshold`；阈值重叠会让宽噪声直线、反光或地图褶皱几乎无条件进入前探补偿。

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
| 直道能走但左右晃动很厉害 | 在 1343 直线基线上小步稳定化：`kp=0.09`、`kd=0.22`、`max_output_delta_per_tick=28`；不要动权重、极性和 `curve_prepare_enable` |
| 直道原地左右转、不往前走 | 先恢复 1343f3c 直线基线：`max_correction=250`、`max_output_delta_per_tick=35`、`curve_prepare_enable=true`、`lead_compensation_enable=false`；不要先测岔路口 |
| 直道遇脏点/褶皱/反光突然变慢 | 优先调 `straight_noise_*`、`sensor_edge_noise_*`、`curve_prepare_confirm_ticks`；不要先关闭 `curve_prepare_enable` 或提高 `base_speed` |
| 宽线约 3 个通道导致小幅左右摆头 | 优先调 `control_error_deadband` / `control_error_soft_zone`，让小偏差不被 PID 放大；不要先大幅改 `kp/kd/max_correction` |
| 0 或 7 通道在无线处偶发亮起 | 保持 `sensor_edge_noise_reject_enable=true`，先调 `sensor_edge_noise_neighbor_threshold`；不要让孤立边缘点刷新位置和方向记忆 |
| 箭头形岔路多通道时间差导致摆头 | 先用 `control_error_*` 降低小偏差响应，并用 `curve_prepare_confirm_ticks=3` 避免单帧误低速；仍走错再单独收紧岔路识别 |
| U 型弯入口冲出 | 先调 `lead_advance_ticks` 让驱动轮轴线中点到达顶点附近，再调 `lead_turn_delta` / `lead_turn_hold_ticks` |
| U 型弯顶点全通道在线后直走丢线 | 检查 `lead_event_min_sum`、`lead_event_active_count_threshold`、`lead_event_center_error_threshold` 是否触发前探补偿 |
| T 字/直角/圆形入口自动进入丢线 | 先调 `lead_advance_ticks`，不要让传感器一看到交点就立即确定性转向 |
| 箭头形干扰路口抖动 | 降低 `interference_active_count_threshold` 或 `interference_position_jump_threshold`，同时确认不会压制真实岔路 |
| 弯道跟不上 | 先确认前探补偿是否足够，再降 `base_speed`、加 `kp`、加 `max_correction` 或调低弯道减速阈值 |
| 丢线后找错方向 | 先查传感器极性和 `sensor_weights`，再调 `edge_hint_threshold` / `direction_update_confidence_min` |
| 雷达无数据 | 查 UART 接线、波特率、IRQ、`radar_enable` |
| 雷达误触发 | 加 `radar_debounce_frames`，减小 `radar_trigger_distance_mm` |
| 快撞上才避障 | 加大 `radar_trigger_distance_mm`，降低 `base_speed`/`obstacle_warn_speed` |
| 绕障转出不够 | 加 `obstacle_turn_out_ms` 或 `obstacle_turn_speed` |
| 绕障没越过障碍 | 加 `obstacle_bypass_ms` 或 `obstacle_bypass_outer_speed` |
| 绕障后找不到线 | 调 `obstacle_turn_in_ms`，加 `obstacle_reacquire_timeout_ms`，降低绕障速度 |

## 11. 参数版本台账与踩坑记录

当前以 `LF_Config_ApplyDebugProfile()` 为上板低速基准：

| 参数组 | 当前值 |
|---|---|
| 传感器 | UART、`sensor_invert_polarity=true`、`sensor_filter_alpha=0.35`、`line_detect_min_sum=780`、`line_detect_min_peak=260`、`line_detect_min_contrast=80` |
| 权重 | `{1750,1250,750,250,-250,-750,-1250,-1750}` |
| PID/速度 | `base_speed=150`、`adaptive_slow_speed=60`、`sharp_turn_speed=35`、`kp=0.09`、`kd=0.22`、`ki=0`、`max_correction=250`、`control_error_deadband=80`、`control_error_soft_zone=240` |
| 输出限制 | `derivative_filter_alpha=0.60`、`max_output_delta_per_tick=28`、`max_motor_cmd=300`、`motor_deadband=0` |
| 策略开关 | `straight_boost_enable=false`、`curve_prepare_enable=true`、`line_stability_enable=true`、`stable_direction_enable=true`、`sensor_edge_noise_reject_enable=true`、`lead_compensation_enable=true`、`fork_enable=false`、`obstacle_avoid_enable=false` |
| 前探补偿 | `lead_event_center_error_threshold=350`、`lead_event_entry_error_threshold=650`、`lead_advance_ticks=14`、`lead_advance_speed=60`、`lead_turn_hold_ticks=7`、`lead_turn_speed=50`、`lead_turn_delta=80` |
| 直线噪声 | `sensor_edge_noise_neighbor_threshold=220`、`straight_noise_confirm_ticks=2`、`straight_noise_active_count_threshold=5`、`straight_noise_max_sum=1800`、`straight_noise_max_position_error=250`、`straight_noise_max_position_delta=120` |
| 速度模式确认 | `curve_prepare_delta_threshold=180`、`curve_prepare_confirm_ticks=3` |

本轮关键结论：线宽约等于 3 个传感器通道时，车稍微放歪或箭头岔路多通道先后扫到线，都会制造小幅位置偏差；如果直接交给 PID 和速度模式，小修正会被放大成左右摆头。优先用 `control_error_*`、`sensor_edge_noise_*` 和 `curve_prepare_confirm_ticks` 降低输入抖动，但死区/软区不能过大，否则会从“抗摆”变成“偏了也不修、弯道也不转”。

本轮不转弯根因：`control_error_deadband=140`、`control_error_soft_zone=420` 对中等偏差压缩过强；同时干扰帧判断会把部分真实偏侧线/弯道线当作跳变干扰，继续用上一可信位置控制。当前修正为 `80/240`，并且强偏侧且有 `edge_hint` 的帧不再作为干扰冻结。

实车参考基线：`56ecd1b5d98ffc8404930787a5c12d5a2e350e49`（2026-05-31 22:31，`fix: 调整控制参数以优化巡线性能，减少误差死区和软区`）目前能基本稳定走到 U 型弯顶点。若后续直线、箭头岔路或 U 型入口效果明显变差，优先与该提交对照代码和参数差异。

当前剩余现象：直线遇箭头型岔路有轻微抖动但不丢线；进入 U 型弯后会多次抖动，随后车头向左过度转向并丢线。该现象更像“弯道内转向响应/车身几何时序问题”，不是直线基础巡线问题。

当前修正方向：在不使用双 PID 的前提下，保守开启已有 `lead_phase` 前探补偿。U 型顶点先低速直行 `lead_advance_ticks=14`，再用更弱更短的 `lead_turn_delta=80`、`lead_turn_hold_ticks=7` 轻转；同时 `frame_is_lead_event()` 增加入口记忆/强特殊线型保护，避免普通宽线或箭头岔路误触发强制转向。

已试过但不好用或只适合特定阶段的参数：

| 版本/参数 | 现象 | 结论 |
|---|---|---|
| `control_error_deadband=140`、`control_error_soft_zone=420` | 直线很稳，但偏离后几乎不修正；可能左轮一直压线前进，弯道入口直接直走。 | 抗摆参数过强会吃掉中等偏差；当前 debug 改为 `80/240`，并用回归测试保护偏侧线仍有明确差速。 |
| `base_speed=100`、`kp=0.08`、`kd=0.20`、`max_correction=50`、`max_output_delta_per_tick=15` | 直线保守，但 U 型弯没有明显右转动作，转向不足，抖动后丢线。 | 不要再用 `max_correction=50` 作为 U 型弯调试基线；它限制了差速能力。 |
| `base_speed=180`、`sharp_turn_speed=40`、`max_correction=100` | 比 100 速更积极，但特殊线型和 U 型弯仍不可靠。 | 单纯加速度或小幅加修正不能解决长前探导致的过早转向。 |
| `sensor_filter_alpha=0.45`、`kd=0.22`、`max_correction=180`、`curve_prepare_enable=false` | 曾作为直线较稳阶段，但 U 型转向不足、特殊线型仍可能丢线。 | 可作为保守参考，不要在此基础上继续同时增大输出限幅和输出变化率。 |
| 1343f3c 直线基线：`kp=0.10`、`kd=0.18`、`max_correction=250`、`max_output_delta_per_tick=35`、`curve_prepare_enable=true` | 基本能走直线，但稳定性较差，晃动厉害；箭头型岔路差点走错线，U 型转弯处剧烈抖动后丢线。 | 当前向稳定方向小步调整为 `kp=0.09`、`kd=0.22`、`max_output_delta_per_tick=28`，并开启 `line_stability_enable/stable_direction_enable` 与更敏感的 `interference_*`。 |
| `lead_event_center_error_threshold=350`、`lead_event_entry_error_threshold=300/350` | center 和 entry 阈值重叠，导致 `frame_is_lead_event()` 的位置条件几乎总为真，宽噪声直线、反光或褶皱可能误进前探补偿。 | `lead_event_entry_error_threshold` 必须明显大于 center 阈值；当前 debug 用 `650`，并有回归测试保护。 |
| 把 debug 权重改成 `{-1750,-1250,-750,-250,250,750,1250,1750}` | 实车直线不稳，箭头岔路摆头走到错误线上，U 型弯进弯会向左转。 | 当前 Yahboom UART 实车通道方向以 `{1750..-1750}` 为准；改权重前必须架空验证左右修正方向。 |
| 仅提高 `kp` 或 `max_correction` 试图救 U 型弯 | 会改善打角，但路口/顶点仍可能因传感器提前 22 cm 看到特殊线型而过早转向或全通道在线后直走。 | 根因是长前探几何，不是纯 PID；优先调 `lead_advance_*`。 |
| 直线突然变慢时优先调 PID 或提高 `base_speed` | 容易掩盖脏点/褶皱/反光误触发，后续在弯道和路口更难稳定。 | 直线误减速优先调 `straight_noise_*`。 |

## 12. 仿真参数选择依据

Simulator 默认 override 仍用于离线 normal/stress 回归；上板验证默认以 debug profile 的低速直线稳定为准。

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

bash simulator/scripts/run_line_follow_stability.sh \
  90 0.01 0.12 20260319 5 \
  simulator/artifacts/line_follow_v1/reports/stability_runs_final \
  simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  "" stress
```

最新 quick normal 验证：手动用当前 `simulator/` 路径构建 runner 后，16 个场景 `overall=95.41/100`，`avg_detect=100.00%`，`max_lost=0.000s`，Confidence `High`。此前 full-course stress 稳定性结果：5 个 seed 均为 High，`min_score=94.45`，`min_detect=99.95%`，`max_lost=0.160s`。

## 13. 每次测试建议记录

```text
日期/场地：
电池电压：
传感器模式和极性：
base_speed/kp/kd/max_correction：
lead_advance_ticks/lead_turn_delta/lead_turn_hold_ticks：
straight_noise_*：
雷达 trigger/release：
避障 turn_out/bypass/turn_in/reacquire：
现象：
下一步：
```
