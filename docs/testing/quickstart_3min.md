# 3 分钟上手（CLI）

本页目标：在本地快速确认巡线回归链路可用。

## 前置条件

- 当前目录为仓库根目录 `TDPS/`
- 系统可用 `bash` 与 `gcc`

## 步骤 1：查看命令帮助

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh help
```

应看到三个子命令：`quick`、`stability`、`run-config`。

## 步骤 2：执行 quick 门禁

```bash
SEED=20260319
REPORT=TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/quick_${SEED}.json

bash TDPS-Simulator/scripts/line_follow_cli.sh quick 15 0.01 0.12 "$REPORT" "$SEED"
```

通过条件（hard gate）：

- `overallScore >= 82`
- `avgLineDetectionRate >= 0.94`
- `maxLongestLostSec <= 0.35`
- 无 runtime error
- 无场景 `score < 70`

退出码：

- `0` 通过
- `1` 门禁失败
- `2` 参数或 I/O 错误

## 步骤 3：查看报告

- quick 报告：`$REPORT`
- 字段说明：`docs/testing/report_schema.md`

## 可选：执行 20-seed 稳定性检查

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh stability 15 0.01 0.12 20260319 20 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/stability_runs
```

稳定性通过条件：

- `minScore >= 80`
- `minDetectPercent >= 93`
- `maxLostSec <= 0.40`

## 可选：雷达与无线专项

```bash
# 雷达帧解析与避障触发
bash TDPS-Simulator/scripts/run_radar_autotest.sh

# LoRa 异步发送/重试 + 三模块联动桩测试
bash TDPS-Simulator/scripts/run_wireless_autotest.sh
```

## 可选：一键系统回归

```bash
bash TDPS-Simulator/scripts/run_system_autotest.sh
```
