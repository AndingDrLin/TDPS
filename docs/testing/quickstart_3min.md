# 3 分钟上手（CLI）

本页目标：在本地快速确认巡线回归链路可用。

## 前置条件

- 当前目录为仓库根目录 `TDPS/`
- 系统可用 `bash` 与 `gcc`

## 步骤 1：查看命令帮助

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh help
```

应看到 `quick`、`full-course`、`stability`、`run-config`。

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

## 可选：完整赛道 normal/stress 检查

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh full-course \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/full_course_normal.json \
  20260319 normal

bash TDPS-Simulator/scripts/line_follow_cli.sh full-course \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/single_run/full_course_stress.json \
  20260319 stress
```

完整赛道应显示 `Full course: passed 4/4`，并且 confidence 为 `High`。

## 可选：执行 20-seed 稳定性检查

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh stability 90 0.01 0.12 20260319 20 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/stability_runs \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  "" stress
```

稳定性通过条件：

- `minScore >= 80`
- `minDetectPercent >= 93`
- `maxLostSec <= 0.40`

## 可选：无线 stub 检查

```bash
mkdir -p firmware/build/gcc

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_wl_stub.c -o firmware/build/gcc/wl_test
./firmware/build/gcc/wl_test
```

## 可选：巡线稳定性回归

```bash
bash TDPS-Simulator/scripts/run_line_follow_stability.sh
```
