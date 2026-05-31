# CLAUDE.md

## Language

Always respond in Chinese (中文).

## Project state

The firmware startup path currently uses `LF_Config_ApplyDebugProfile()` by default. Use the low-speed conservative debug profile unless the user explicitly asks to switch to `LF_Config_ApplyCompetitionProfile()`.

## 巡线几何约束

当前三轮差速车的传感器阵列中心到驱动轮轴线中点约 22 cm，左右驱动轮中心距约 16 cm。处理路口、U 型弯顶点、直角转弯和圆形转弯入口时，以左右驱动轮轴线中点作为实际转向参考点；前置传感器刚看到特殊线型时，不应立即执行确定性转向。

优先调 `lead_advance_*`，让左右驱动轮轴线中点到达特殊位置附近后再转向；然后再调 `lead_turn_delta` 和 `lead_turn_hold_ticks`。直线上的地面脏点、地图褶皱和反光干扰，优先调 `straight_noise_*`，不要优先改 PID 或提高 `base_speed`。
