# Phase 0 Baseline 冻结记录

冻结日期：2026-03-20

## 目的

为后续迭代提供可追踪基线，并启用 `<= 5%` 回归保护阈值。

## 执行命令

```bash
bash TDPS-Simulator/scripts/run_line_follow_autotest.sh \
  15 0.01 0.12 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/baseline/phase0_quick_20260319.json \
  20260319

bash TDPS-Simulator/scripts/run_line_follow_stability.sh \
  15 0.01 0.12 \
  20260319 20 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/baseline/stability_20260319
```

## quick 结果

- Exit code: `0`
- Report: `TDPS-Simulator/artifacts/line_follow_v1/reports/baseline/phase0_quick_20260319.json`
- `overallScore`: `92.07`
- `avgLineDetectionRate`: `99.70%`
- `maxLongestLostSec`: `0.070s`
- Runtime errors: `0`
- Scenarios with `score < 70`: `0`

## stability 结果（20 seeds）

- Exit code: `0`
- Report dir: `TDPS-Simulator/artifacts/line_follow_v1/reports/baseline/stability_20260319`
- `minScore`: `91.28`
- `minDetectPercent`: `99.07%`
- `maxLostSec`: `0.070s`

## 后续使用规则

- baseline 对比只用于同一场景集、同一参数口径。
- 若场景集或评分规则变更，必须重新冻结 baseline。
- 新 baseline 需写入本文件并保留旧记录，不覆盖历史结论。
