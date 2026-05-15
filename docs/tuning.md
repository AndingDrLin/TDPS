# 整车测试调参说明

整车测试时，手动调整的参数集中在 `firmware/Src/lf_config.c` 的 `g_lf_config`。不要分散修改算法文件。每次只改 1-2 个参数，记录“场地/电池电压/参数/现象/结果”。

## 推荐调试顺序

1. 传感器极性和阈值
2. 基础速度 `base_speed`
3. PID：先 `kp`，再 `kd`，最后通常不启用 `ki`
4. 丢线恢复
5. 雷达 WARN/BLOCK 阈值
6. 避障动作时间和速度
7. LoRa 检查点联调

## 1. 主循环与启动参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `control_period_ms` | `10` | 巡线控制周期。越小响应越快，但 CPU/串口调试压力更大。 | 默认保持 10ms，不建议现场改。若电机响应明显慢，优先调 PID 和速度。 |
| `auto_start_delay_ms` | `1200` | 上电后未按启动键时自动开始标定的等待时间。 | 需要更多摆车/放车时间就加大；想快速开跑就减小。 |

## 2. 标定参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `calibration_duration_ms` | `3000` | 标定总时长，用于采集每路传感器黑白 min/max。 | 标定后经常误判黑白时加长；场地光照稳定可略缩短。 |
| `calibration_switch_interval_ms` | `300` | 标定时左右摆动切换周期。 | 摆动覆盖不到黑线两侧时增大；摆动太慢时减小。 |
| `calibration_spin_speed` | `180` | 标定时原地摆动速度。 | 传感器扫不过黑白边界就增大；车体抖动或滑出线就减小。 |

## 3. 传感器参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `sensor_input_mode` | `LF_SENSOR_INPUT_ANALOG_ADC` | 8 路传感器输入模式。ADC 用模拟量，GPIO 用数字量。 | 实机接 ADC 就保持；如果 8-LP 用 IO 模式，改为 `LF_SENSOR_INPUT_DIGITAL_GPIO`。 |
| `sensor_invert_polarity` | `false` | 模拟量极性翻转。 | 黑线原始值比白底低时改为 `true`；如果位置方向反、在线判断反，优先查此项。 |
| `sensor_digital_threshold` | `2048` | 数字/GPIO 判高低的阈值。 | 仅数字模式相关。临界抖动时远离黑白中间值。 |
| `sensor_digital_active_high` | `false` | 数字模式下“在线”是否为高电平。 | 黑线时 IO 输出高则设 `true`，黑线时输出低则保持 `false`。 |
| `sensor_use_dynamic_calibration` | `true` | 是否启用上电动态标定。 | ADC 模式建议开启；数字模式或标定动作不方便时关闭。 |
| `sensor_filter_alpha` | `0.35` | 低通滤波系数。越大越灵敏，越小越平滑。 | 车左右抖动、传感器噪声大就减小；弯道反应慢、跟线滞后就增大。 |
| `line_detect_min_sum` | `780` | 判定“检测到线”的总强度阈值。 | 经常误丢线就降低；白底也被认为在线就升高。先确认极性再调。 |
| `edge_hint_threshold` | `120` | 左右半区强度差超过该值时更新丢线恢复方向。 | 丢线后总往错边找线就检查权重/极性，再调此值；方向变化太敏感就升高。 |
| `sensor_weights` | `{-1750..1750}` | 8 路横向位置权重，左负右正。 | 传感器物理间距不均或安装偏移时微调；一般不先改。 |

## 4. 巡线 PID 与电机参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `base_speed` | `320` | 正常巡线基础速度。 | 第一优先调。直道稳定后逐步加；弯道冲出或避障不稳就降。 |
| `kp` | `0.48` | 比例增益，决定偏离黑线后的纠正力度。 | 贴不住弯、回正慢就加；左右蛇形摆动就减。 |
| `kd` | `1.90` | 微分增益，抑制快速摆动。 | 加 `kp` 后抖动就加 `kd`；转弯变迟钝或电机尖叫就减。 |
| `ki` | `0.0` | 积分增益，修长期偏差。 | 通常保持 0。只有持续偏一侧且机械问题排除后，再小幅增加。 |
| `max_correction` | `340` | PID 修正量限幅。 | 弯道打角不够就加；急转导致甩尾或左右轮打满就减。 |
| `max_motor_cmd` | `900` | 电机命令绝对值上限。 | 保护电机/电池。速度不够可加，但先确认机械和供电。 |
| `motor_deadband` | `120` | 非零命令的最小起转补偿。 | 小命令车不动就加；低速动作太冲就减。 |

## 5. 丢线恢复参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `recover_turn_speed` | `220` | 丢线后原地搜索速度。 | 找线太慢就加；转太猛错过线就减。 |
| `recover_timeout_ms` | `900` | 丢线恢复最长时间，超时停车。 | 短暂离线即可找回但停车太早就加；失控转圈太久就减。 |

## 6. 雷达判定参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `radar_enable` | `true` | 是否启用雷达解析和避障状态。 | 单独调巡线时可临时设 `false`。整车联调必须开启。 |
| `radar_uart_baudrate` | `256000` | LD2410S 串口波特率记录值。实际 UART 初始化仍需与 CubeMX/板级配置一致。 | 如果完全无雷达帧，先确认 CubeMX USART 波特率、接线和中断。 |
| `radar_trigger_distance_mm` | `450` | 进入 `BLOCK` 的距离阈值。 | 车来不及避障就加大；离障碍很远就绕行就减小。 |
| `radar_release_distance_mm` | `650` | 从 `BLOCK/WARN` 释放到 `CLEAR` 的距离阈值。应大于 trigger，形成滞回。 | 状态反复跳变就加大与 trigger 的差值；解除太慢就减小。 |
| `radar_debounce_frames` | `3` | 连续多少帧确认状态变化。 | 误报多就加；反应慢就减。 |
| `radar_frame_timeout_ms` | `120` | 超过该时间没有有效帧则清除雷达状态。 | 雷达帧率低导致状态闪断就加；断线后旧状态保持太久就减。 |

## 7. 避障参数

| 参数 | 当前值 | 含义 | 调试方法 |
|---|---:|---|---|
| `obstacle_warn_speed` | `220` | `WARN` 状态下的最高巡线速度。 | 接近障碍仍太快就减；减速过早影响通行就加。 |
| `obstacle_avoid_enable` | `true` | 是否启用 `BLOCK` 后自动绕障。 | 只验证雷达停车时可设 `false`；整车任务设 `true`。 |
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

## 8. 实车现象到参数的快速映射

| 现象 | 优先检查/调整 |
|---|---|
| 上电标定后一直认为丢线 | `sensor_invert_polarity`、`sensor_digital_active_high`、`line_detect_min_sum` |
| 直道左右蛇形 | 降 `kp`，或加 `kd`，或降 `base_speed` |
| 弯道跟不上 | 降 `base_speed`，加 `kp`，加 `max_correction` |
| 丢线后找错方向 | 先查传感器极性和 `sensor_weights`，再调 `edge_hint_threshold` |
| 雷达无数据 | 查 UART 接线、波特率、IRQ、`radar_enable` |
| 雷达误触发 | 加 `radar_debounce_frames`，减小 `radar_trigger_distance_mm` |
| 快撞上才避障 | 加大 `radar_trigger_distance_mm`，降低 `base_speed`/`obstacle_warn_speed` |
| 绕障转出不够 | 加 `obstacle_turn_out_ms` 或 `obstacle_turn_speed` |
| 绕障没越过障碍 | 加 `obstacle_bypass_ms` 或 `obstacle_bypass_outer_speed` |
| 绕障后找不到线 | 调 `obstacle_turn_in_ms`，加 `obstacle_reacquire_timeout_ms`，降低绕障速度 |

## 9. 仿真参数选择依据

当前默认参数不是单次最快值，而是在 Simulator normal/stress profile 下优先选择丢线时间短、完整赛道通过且压力扰动有裕量的组合：

- `base_speed=320`
- `kp=0.48`
- `kd=1.90`
- `max_correction=340`

已验证命令示例：

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick 15 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/quick_final.json \
  20260319 TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_default.csv normal

bash TDPS-Simulator/scripts/line_follow_cli.sh full-course \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/full_course_final_stress.json \
  20260319 stress

bash TDPS-Simulator/scripts/run_line_follow_stability.sh \
  90 0.01 0.12 20260319 5 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/stability_runs_final \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  "" stress
```

最近一次压力稳定性结果：5 个 seed 均为 High，`min_score=94.45`，`min_detect=99.95%`，`max_lost=0.160s`。

## 10. 每次测试建议记录

```text
日期/场地：
电池电压：
传感器模式和极性：
base_speed/kp/kd：
雷达 trigger/release：
避障 turn_out/bypass/turn_in/reacquire：
现象：
下一步：
```
