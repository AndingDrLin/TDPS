# 回归报告字段定义

本文档描述两个 JSON 输出：

- quick 报告：`lf_autotest_report_v4`
- stability 汇总：`lf_stability_summary_v1`

## 1. quick 报告（`lf_autotest_report_v4`）

### 1.1 顶层字段

- `version` (number)：当前固定 `4`
- `schema` (string)：`lf_autotest_report_v4`
- `generatedAtEpochSec` (number)：生成时间戳（秒）
- `config` (object)：本次运行参数
- `summary` (object)：聚合指标
- `confidence` (object)：置信度分级
- `baselineComparison` (object)：baseline 对比结果
- `issues` (array[string])：门禁问题列表
- `scenarios` (array[object])：逐场景指标

### 1.2 `config`

- `durationSec` (number)
- `dt` (number)
- `lineThreshold` (number)
- `baseSeed` (uint32)
- `disturbanceProfile` (`normal|stress`)

### 1.3 `summary`

- `scenarioCount` (uint32)
- `completedCount` (uint32)
- `aborted` (bool)
- `overallScore` (0..100)
- `avgLineDetectionRate` (0..1)
- `maxLongestLostSec` (seconds)

### 1.4 `confidence`

- `level` (`High|Medium|Low`)
- `profile` (`normal|stress`)
- `ruleVersion` (string，当前 `lf_confidence_v1`)
- `passedHardGate` (bool)
- `score` (number)
- `detectPercent` (number)
- `maxLostSec` (number)
- `thresholds.high.{minScore,minDetectPercent,maxLostSec}`
- `thresholds.medium.{minScore,minDetectPercent,maxLostSec}`

### 1.5 `baselineComparison`

- `enabled` (bool)
- `loaded` (bool)
- `withinRegressionGate` (bool)
- `regressionGatePct` (number，当前 5.0)
- `baselineReportPath` (string|null)
- `baselineOverallScore` (number)
- `baselineAvgLineDetectionRate` (number)
- `baselineMaxLongestLostSec` (number)
- `currentOverallScore` (number)
- `currentAvgLineDetectionRate` (number)
- `currentMaxLongestLostSec` (number)
- `scoreRegressionPct` (number)
- `detectionRegressionPct` (number)
- `maxLostRegressionPct` (number)
- `error` (string|null)

说明：未启用 baseline 时，`enabled=false`，其余对比字段为默认值。

### 1.6 `scenarios[*]`

- `id` (string)
- `name` (string)
- `preset` (string)
- `durationTargetSec` (number)
- `durationSimulatedSec` (number)
- `steps` (uint32)
- `lineDetectionRate` (0..1)
- `lineLostTransitions` (uint32)
- `lineRecoveredTransitions` (uint32)
- `longestLostSec` (seconds)
- `totalLostSec` (seconds)
- `meanAbsErrorM` (meters)
- `rmsErrorM` (meters)
- `maxAbsErrorM` (meters)
- `motorSaturationRate` (0..1)
- `distanceM` (meters)
- `runtimeError` (string|null)
- `score` (0..100)

## 2. stability 汇总（`lf_stability_summary_v1`）

文件路径：`<report_dir>/stability_summary.json`

### 2.1 顶层字段

- `version` (number)：当前固定 `1`
- `schema` (string)：`lf_stability_summary_v1`
- `generatedAtEpochSec` (number)
- `config` (object)
- `summary` (object)
- `gates` (object)
- `confidence` (object)

### 2.2 `config`

- `durationSec`
- `dt`
- `lineThreshold`
- `seedStart`
- `runs`
- `disturbanceProfile`

### 2.3 `summary`

- `minScore`
- `minDetectPercent`
- `maxLostSec`

### 2.4 `gates`

- `minScore`（当前 `80`）
- `minDetectPercent`（当前 `93`）
- `maxLostSec`（当前 `0.40`）

### 2.5 `confidence`

结构与 quick 报告中的 `confidence` 一致。
