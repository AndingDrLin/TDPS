# TDPS

TDPS 包含两个子系统：
- `code/line_follow_v1`: Task 1 巡线控制（8 路灰度 + 雷达避障状态接入 + 仿真回归）
- `code/wireless_v1`: Task 2 无线通信（LoRa 透传 + 异步队列 + 超时重试 + 可选 ACK）

## 快速测试

```bash
# 巡线 quick 门禁
bash TDPS-Simulator/scripts/line_follow_cli.sh quick

# 雷达帧解析专项
bash TDPS-Simulator/scripts/run_radar_autotest.sh

# 无线模块 + 巡线集成自动化测试
bash TDPS-Simulator/scripts/run_wireless_autotest.sh

# 一键全量回归（quick + radar + wireless + integration）
bash TDPS-Simulator/scripts/run_system_autotest.sh
```

## 关键行为

- 巡线输入升级为 8 路灰度，支持标定、归一化、滤波、加权位置估计和丢线辅助方向。
- 雷达采用串口帧解析（保守默认帧格式），输出 `CLEAR/WARN/BLOCK` 避障状态，不阻塞主控制环。
- 无线发送由异步队列驱动，支持超时重试与可选 ACK，主循环通过 `Tick` 非阻塞推进。
- 集成入口：`LF_App_NotifyCheckpoint(checkpoint_id)`。
- 报文格式：`TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<ms>\n`。

## 文档

- 巡线模块说明：`code/line_follow_v1/README.md`
- 无线模块说明：`code/wireless_v1/README.md`
- 无线设计文档：`docs/wireless_comm.md`
- 巡线仿真说明：`TDPS-Simulator/sim_tests/line_follow_v1/README.md`
- 无线仿真说明：`TDPS-Simulator/sim_tests/wireless_v1/README.md`
- 联动仿真说明：`TDPS-Simulator/sim_tests/integration/README.md`
