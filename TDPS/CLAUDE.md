# CLAUDE.md

## Language

Always respond in Chinese (中文).

## Project state

The firmware startup path currently uses `LF_Config_ApplyDebugProfile()` by default. Use the low-speed conservative debug profile unless the user explicitly asks to switch to `LF_Config_ApplyCompetitionProfile()`.

## 巡线控制架构

当前使用 **6 参数简化 PD + 曲率前馈(kff)** 架构（`TDPS_SIMPLE_CONTROL=1`，见 `lf_app.c:1`）：

- 核心公式：`correction = kp*error + kd*derivative + kff*derivative*speed`
- 连续速度函数：`speed = base_speed - (base_speed - min_speed) * |error| / 1750`（IIR 平滑 alpha=0.4）
- 6 个可调参数：`kp=0.25`, `kd=1.20`, `kff=0.0008`, `base_speed=280`, `min_speed=60`, `max_correction=300`
- 已关闭：死区/软区/积分/速度缩放/变化率限幅/二重滤波/straight_boost/curve_prepare
- 如切回旧架构：`TDPS_SIMPLE_CONTROL=0`

## 巡线几何约束

当前三轮差速车的传感器阵列中心到驱动轮轴线中点约 22 cm，左右驱动轮中心距约 16 cm。处理路口、U 型弯顶点、直角转弯和圆形转弯入口时，以左右驱动轮轴线中点作为实际转向参考点；前置传感器刚看到特殊线型时，不应立即执行确定性转向。

优先调 `lead_advance_*`，让左右驱动轮轴线中点到达特殊位置附近后再转向；然后再调 `lead_turn_delta` 和 `lead_turn_hold_ticks`。直线上的地面脏点、地图褶皱和反光干扰，优先调 `straight_noise_*`，不要优先改 PID 或提高 `base_speed`。
