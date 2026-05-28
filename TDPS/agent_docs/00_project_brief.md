# TDPS 项目简报

## 项目目标

TDPS 是 Team 15 智能巡线小车项目，目标 MCU 为 STM32F407VET6。当前目标是在统一控制框架下完成三类任务：

1. 巡线行驶：沿 3 cm 黑线稳定行驶，支持弯道、方框区域和丢线恢复。
2. LoRa 检查点通信：到达检查点时发送 `TEAM/NAME/CP/TIME` 报文，通信不能阻塞巡线控制。
3. 雷达避障：通过 HLK-LD2410S 判断障碍状态，在障碍区前做 WARN 减速或 BLOCK 绕障/停车策略。

## 当前主目录

```text
TDPS/
  firmware/             # 主固件工程
  simulator/            # 离线仿真与回归测试
  tests/                # 板级测试和独立外设测试
  agent_docs/           # agent/Claude 快速上下文
  docs/                 # 手写设计、测试、调参文档
  docs_reference/       # 本地硬件手册和厂商资料，Git 只跟踪索引
  reports/              # 报告和实验记录脚本
  archive/experiments/  # 临时/历史实验源码
```

## 关键入口

- 固件：`TDPS/firmware/`
- 固件 README：`TDPS/README.md`
- 总体设计：`TDPS/docs/basic_design.md`
- 巡线说明：`TDPS/docs/line_following.md`
- 整车调参：`TDPS/docs/tuning.md`
- 无线通信：`TDPS/docs/wireless_comm.md`
- 板级测试：`TDPS/tests/board/tdps_board_test/`
- 雷达独立测试：`TDPS/tests/standalone/radar_ld2410s/`
- 历史 monitor 实验：`TDPS/archive/experiments/monitor_radar_screen/`

## 重要约束

- 主控制结构是“主循环 + 外设中断 + 状态机”，不引入 RTOS。
- 巡线控制周期不能被 LoRa 或雷达逻辑阻塞。
- 参数优先集中改 `TDPS/firmware/Src/lf_config.c`，不要分散改算法文件。
- 当前雷达主要提供前向距离/状态；若要完成左右分叉路线选择，需要额外确认左右侧障碍检测能力或传感器布置。
