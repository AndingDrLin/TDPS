# 回归报告字段定义

本文档描述两个 JSON 输出：

- quick/full-course 报告：`lf_autotest_report_v6`
- stability 汇总：`lf_stability_summary_v1`

## 1. quick/full-course 报告（`lf_autotest_report_v6`）

### 1.1 顶层字段

- `version` (number)：当前固定 `6`
- `schema` (string)：`lf_autotest_report_v6`
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
- `fullCourse.scenarioCount` (uint32)：完整赛道场景数量。
- `fullCourse.passedCount` (uint32)：通过完整赛道 hard gate 的场景数量。
- `fullCourse.allPassed` (bool)：完整赛道场景是否全部通过。
- `fullCourse.minProgressPercent` (number)：完整赛道最小路线进度百分比。
- `fullCourse.maxFinishTimeSec` (seconds)：完整赛道最大完成时间。

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
- `taskScore` (0..100)：任务语义评分，覆盖 finish、collision、checkpoint、boundary、obstacle。
- `task.finished` (bool)：是否进入 finish 区域。
- `task.finishTimeSec` (seconds)：进入 finish 区域的仿真时间；未完成时为 0。
- `task.collided` (bool)：是否与障碍物碰撞。
- `task.boundaryViolationCount` (uint32)：越出 patio corridor 的连续事件次数。
- `task.boundaryViolationSteps` (uint32)：越出 patio corridor 的采样步数。
- `course.enabled` (bool)：是否启用完整赛道语义。
- `course.fullCoursePassed` (bool)：是否通过完整赛道 hard gate。
- `course.validFinish` (bool)：是否满足进度、checkpoint 顺序、边界、碰撞等前置条件后到达终点。
- `course.progressM` (meters)：当前沿赛道中心线投影进度。
- `course.maxProgressM` (meters)：本场景达到过的最大进度。
- `course.courseLengthM` (meters)：赛道中心线长度。
- `course.progressPercent` (0..100)：`maxProgressM/courseLengthM`。
- `course.boundaryViolationSec` (seconds)：完整赛道 corridor 越界累计时间。
- `obstacles.count` (uint32)：场景障碍物数量。
- `obstacles.detected` (uint32)：雷达/固件处于 WARN/BLOCK 的累计步数。
- `obstacles.avoidStarted` (uint32)：进入 `AVOID_*` 避障状态机次数。
- `obstacles.avoidCompleted` (uint32)：避障后回到 RUNNING 的次数。
- `checkpoints.expected` (uint32)：场景期望检查点数量。
- `checkpoints.triggered` (uint32)：首次触发的检查点数量。
- `checkpoints.missed` (uint32)：未触发检查点数量。
- `checkpoints.duplicates` (uint32)：重复进入同一检查点区域次数。
- `checkpoints.outOfOrder` (uint32)：完整赛道中乱序触发检查点次数。
- `checkpoints.lastId` (uint32)：最后一次进入的检查点 ID，未触发时为 0。
- `radar.detections` (uint32)：Simulator 生成雷达帧次数。
- `wireless.enabled` (bool)：场景是否启用无线模块。
- `wireless.queueDepth` (uint16)：LoRa 异步队列深度。
- `wireless.queueDropped` (uint16)：LoRa 队列溢出丢弃计数。
- `wireless.txSuccess` (uint16)：LoRa stub 成功发送计数。
- `wireless.txFail` (uint16)：LoRa stub 失败计数。
- `wireless.retryCount` (uint16)：LoRa 重试计数。
- `wireless.checkpointEnqueued` (uint16)：checkpoint 报文成功入队计数。
- `wireless.checkpointEnqueueFail` (uint16)：checkpoint 报文入队失败计数。
- `wireless.checkpointThrottled` (uint16)：checkpoint 报文被应用层节流计数。
- `runtimeError` (string|null)
- `score` (0..100)：保留巡线评分，旧门禁仍使用该字段。

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
