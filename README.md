# TDPS Project

> 基于 STM32 的智能小车项目：巡线 + LoRa 通信 + 雷达避障

## Tasks

- **任务 1：巡线行驶**：沿黑线稳定通过弯道与路口，到达终点。
- **任务 2：无线通信**：在指定拱门区域通过 LoRa 发送队伍与计时信息。
- **任务 3：避障通行**：使用 24 GHz 及以上雷达判断障碍左右并选择空侧通过。

## Content

1. 总体设计与实现规划 [basic_design.md](docs/basic_design.md)。
2. 巡线子系统设计文档 [line_following.md](docs/line_following.md)

## Reference

1. [SayanSeth/Line-Follower-Robot](https://github.com/SayanSeth/Line-Follower-Robot)
2. [sametoguten/STM32-Line-Follower-with-PID](https://github.com/sametoguten/STM32-Line-Follower-with-PID)
3. [navidadelpour/line-follower-robot](https://github.com/navidadelpour/line-follower-robot)
4. [FredBill1/RT1064_Smartcar](https://github.com/FredBill1/RT1064_Smartcar)
5. [ittuann/Enterprise_E](https://github.com/ittuann/Enterprise_E)
6. [Awesome-IntelligentCarRace](https://ittuann.github.io/Awesome-IntelligentCarRace/)