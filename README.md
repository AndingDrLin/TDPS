# TDPS

`TDPS` 是一个基于 STM32 的智能小车项目仓库。当前可直接使用的主线是 `line_follow_v1` 巡线系统，以及配套的离线回归测试（CLI-first）。

## 当前范围

- 巡线控制主流程：标定、巡线、丢线恢复、超时停车。
- 离线回归：`quick`、`stability`、`run-config` 三类命令。
- STM32F4 HAL 端口：可直接接入 Cube 工程。
- LoRa 与雷达：保留事件钩子，未在本仓库实现具体业务逻辑。

## 快速开始

在仓库根目录执行：

```bash
# 1) 查看命令
bash TDPS-Simulator/scripts/line_follow_cli.sh help

# 2) 单次门禁（quick）
bash TDPS-Simulator/scripts/line_follow_cli.sh quick

# 3) 20-seed 稳定性门禁（stability）
bash TDPS-Simulator/scripts/line_follow_cli.sh stability 15 0.01 0.12 20260319 20 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/stability_runs
```

默认环境：`bash` + `gcc`（用于构建 `TDPS-Simulator/artifacts/line_follow_v1/bin/lf_autotest_runner`）。

## 门禁标准

### quick（单次）

- `overallScore >= 82`
- `avgLineDetectionRate >= 0.94`
- `maxLongestLostSec <= 0.35`
- 无 runtime error
- 所有场景 `score >= 70`

### stability（多 seed）

- `minScore >= 80`
- `minDetectPercent >= 93`
- `maxLostSec <= 0.40`

### 退出码约定

- `0`：通过
- `1`：门禁失败
- `2`：参数错误 / I/O 错误 / 报告写入失败

## 文档导航

### 使用与回归

- [3 分钟上手（CLI）](docs/testing/quickstart_3min.md)
- [10 分钟完整回归（CLI）](docs/testing/full_regression_10min.md)
- [CLI 迁移说明](docs/testing/migration.md)
- [报告字段定义（Schema）](docs/testing/report_schema.md)
- [基线冻结记录](docs/testing/baseline.md)

### 代码说明

- [line_follow_v1 模块说明](code/line_follow_v1/README.md)
- [line_follow_v1 仿真测试说明](TDPS-Simulator/sim_tests/line_follow_v1/README.md)

### 设计文档

- [总体设计](docs/basic_design.md)
- [巡线子系统设计](docs/line_following.md)

## 目录结构

```text
TDPS/
├── code/line_follow_v1/          # 巡线一期固件实现
├── docs/                         # 设计与回归文档
├── TDPS-Simulator/
│   ├── scripts/                  # 仿真测试 CLI 与构建脚本
│   ├── sim_tests/                # 离线仿真测试代码（common + suite）
│   └── artifacts/                # 仿真产物（bin / reports / logs）
└── slides/                       # 课题展示材料
```

## 板级接入（STM32CubeIDE）

实机烧录请直接看：[`code/line_follow_v1/README.md`](code/line_follow_v1/README.md)

核心要求：

- 编译宏启用 `LF_USE_STM32F4_HAL_PORT`
- 按接线修改 `Inc/lf_port_stm32f4_hal.h`
- 在工程中提供 `g_lf_sensor_dma_buffer`、`LF_Port_SystemClock_Config`、`LF_Port_Peripheral_Init`

## 生成物

以下产物统一输出到 `TDPS-Simulator/artifacts/line_follow_v1/`：

- `bin/lf_autotest_runner`
- `reports/`
- `logs/*.log`
