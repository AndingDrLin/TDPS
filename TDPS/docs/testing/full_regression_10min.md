# 10 分钟完整回归（CLI）

本页给出一套可直接用于本地和 CI 的完整回归流程：`quick` + `stability(normal)` + `stability(stress)` + 复现性检查。

## 0. 统一变量

```bash
SEED=20260319
DURATION=15
DT=0.01
THRESH=0.12

REG_DIR=TDPS-Simulator/artifacts/line_follow_v1/reports/regression
mkdir -p "$REG_DIR"
```

## 1. 构建 runner

```bash
bash TDPS-Simulator/scripts/build_line_follow_runner.sh
```

构建参数固定为 `-std=c11 -Wall -Wextra -Werror`。

## 2. quick（可选 baseline 对比）

### 2.1 不做 baseline

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick "$DURATION" "$DT" "$THRESH" \
  "$REG_DIR/quick.json" "$SEED"
```

### 2.2 做 baseline 对比

`quick` 直连 CLI 时，若需要 baseline，需显式提供场景配置路径。

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick "$DURATION" "$DT" "$THRESH" \
  "$REG_DIR/quick_vs_baseline.json" "$SEED" \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_default.csv \
  TDPS-Simulator/artifacts/line_follow_v1/reports/baseline/phase0_quick_20260319.json
```

baseline 对比通过条件：三项回归比例均 `<= 5%`。

## 3. stability（normal, 20 seeds）

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh stability "$DURATION" "$DT" "$THRESH" \
  "$SEED" 20 "$REG_DIR/stability_normal_20"
```

通过条件：

- `minScore >= 80`
- `minDetectPercent >= 93`
- `maxLostSec <= 0.40`

输出：

- `report_seed_*.json`
- `stability_summary.json`

## 4. stability（stress, 20 seeds）

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh stability "$DURATION" "$DT" "$THRESH" \
  "$SEED" 20 "$REG_DIR/stability_stress_20" \
  TDPS-Simulator/sim_tests/line_follow_v1/config/scenarios_default.csv stress
```

说明：

- `stress` 用于揭示薄弱点，可单独作为诊断流水线。
- 如用于观察而非拦截，可在 CI 中对该步骤使用 `|| true`。

## 5. 复现性检查（同 seed）

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick "$DURATION" "$DT" "$THRESH" "$REG_DIR/repro_a.json" "$SEED"
bash TDPS-Simulator/scripts/line_follow_cli.sh quick "$DURATION" "$DT" "$THRESH" "$REG_DIR/repro_b.json" "$SEED"

sed '/"generatedAtEpochSec"/d' "$REG_DIR/repro_a.json" > /tmp/repro_a.norm
sed '/"generatedAtEpochSec"/d' "$REG_DIR/repro_b.json" > /tmp/repro_b.norm
cmp -s /tmp/repro_a.norm /tmp/repro_b.norm
```

`cmp` 退出码为 `0` 表示复现通过（忽略时间戳字段）。

## 6. 退出码约定

- `0`：通过
- `1`：门禁失败
- `2`：参数错误 / 文件错误 / 报告写入失败
