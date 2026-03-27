# TDPS

TDPS 包含两个子系统：
- `code/line_follow_v1`: Task 1 巡线控制（含仿真回归）
- `code/wireless_v1`: Task 2 无线通信（LoRa 检查点报文）

## 快速测试

```bash
# 巡线 quick 门禁
bash TDPS-Simulator/scripts/line_follow_cli.sh quick

# 无线模块 + 巡线集成自动化测试
bash TDPS-Simulator/scripts/run_wireless_autotest.sh
```

## 关键行为

- 无线功能仅实现任务要求：`checkpoint -> LoRa packet`。
- 集成入口：`LF_App_NotifyCheckpoint(checkpoint_id)`。
- 报文格式：`TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<ms>\n`。

## 文档

- 巡线模块说明：`code/line_follow_v1/README.md`
- 无线模块说明：`code/wireless_v1/README.md`
- 无线设计文档：`docs/wireless_comm.md`
- 巡线仿真说明：`TDPS-Simulator/sim_tests/line_follow_v1/README.md`
- 无线仿真说明：`TDPS-Simulator/sim_tests/wireless_v1/README.md`
