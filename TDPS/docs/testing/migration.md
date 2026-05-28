# CLI 迁移说明

## 背景

仓库已统一到 `TDPS-Simulator/scripts/line_follow_cli.sh`，并保留旧脚本兼容入口。

## 新旧入口

### 新入口（推荐）

- `bash TDPS-Simulator/scripts/line_follow_cli.sh quick ...`
- `bash TDPS-Simulator/scripts/line_follow_cli.sh stability ...`
- `bash TDPS-Simulator/scripts/line_follow_cli.sh run-config ...`

### 兼容入口（保留）

- `bash TDPS-Simulator/scripts/run_line_follow_autotest.sh ...`
- `bash TDPS-Simulator/scripts/run_line_follow_stability.sh ...`

兼容入口内部仍调用同一 runner，不会分叉逻辑。

## 参数映射

### quick

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick \
  [duration] [dt] [line_threshold] [report_path] [base_seed] \
  [scenario_config] [baseline_report] [profile]
```

### stability

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh stability \
  [duration] [dt] [line_threshold] [seed_start] [runs] [report_dir] \
  [scenario_config] [baseline_report] [profile]
```

### run-config

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh run-config \
  <config_path> [duration] [dt] [line_threshold] [report_path] [base_seed] \
  [baseline_report] [profile]
```

## 可选参数顺序规则

- `profile` 只能是 `normal|stress`，且位于可选参数最后。
- `quick/stability` 里，若要传 `baseline_report`，建议同时显式传 `scenario_config`。
- 若只想用默认场景做 baseline 对比，优先使用兼容脚本（脚本会自动补默认场景路径）。

## 报告变化

- quick 报告 schema：`lf_autotest_report_v4`
- stability 汇总 schema：`lf_stability_summary_v1`
- quick/stability 均输出 `confidence` 字段（`High|Medium|Low`）

## 推荐迁移步骤

1. 把调用入口替换到 `line_follow_cli.sh`。
2. 在 CI 中显式检查退出码（`0/1/2`）。
3. 将解析逻辑改为读取 `docs/testing/report_schema.md` 定义字段。
4. 兼容脚本只保留给历史任务或人肉调试。
