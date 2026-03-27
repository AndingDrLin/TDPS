# TDPS

`TDPS` 是一个基于 STM32 的智能小车项目仓库。当前包含两个主要子系统：

1. **`line_follow_v1`** — 巡线系统（Task 1），含配套离线回归测试
2. **`wireless_v1`** — 无线通信系统（Task 2），通过 LoRa 模块向拱门发送检查点报文

## 硬件清单

| 元件 | 型号 / 规格 |
|------|------------|
| MCU | STM32F103C8T6 |
| 电池 | 2200mAh 4S 14.8V 150C（Ovonic，XT60 插头） |
| 24GHz 雷达 | CYCPLUS 雷达芯片（课程提供） |
| 无线通信模块 | EWM22A-900BWL22S（LoRa 850–930 MHz，UART 接口） |
| 显示屏 | 4 英寸车载 OLED 屏幕 |
| 电机驱动 | L298N |
| 循迹传感器 | 红外循迹模块 |

## 当前范围

### Task 1 — 巡线（line_follow_v1）
- 巡线控制主流程：标定、巡线、丢线恢复、超时停车
- 离线回归：`quick`、`stability`、`run-config` 三类命令
- STM32F4 HAL 端口：可直接接入 Cube 工程
- LoRa 与雷达：保留事件钩子

### Task 2 — 无线通信（wireless_v1）
- EWM22A LoRa 模块 AT 指令配置与透传数据发射
- 拱门检查点报文打包（队号、队名、检查点 ID、时间戳）
- 比赛计时与发射节流控制
- 分层架构：应用层 → 协议层 / 驱动层 → 平台抽象层
- 支持 PC 端桩测试与 STM32 真实硬件运行

## 快速开始

### 巡线模块测试

```bash
# 1) 查看命令
bash TDPS-Simulator/scripts/line_follow_cli.sh help

# 2) 单次门禁（quick）
bash TDPS-Simulator/scripts/line_follow_cli.sh quick

# 3) 20-seed 稳定性门禁（stability）
bash TDPS-Simulator/scripts/line_follow_cli.sh stability 15 0.01 0.12 20260319 20 \
  TDPS-Simulator/artifacts/line_follow_v1/reports/stability_runs
```

### 无线模块测试（PC 桩模式）

```bash
cd code/wireless_v1/Src
gcc -I../Inc -o wl_test main.c wl_app.c wl_lora.c wl_protocol.c wl_config.c wl_platform_stub.c
./wl_test
```

默认环境：`bash` + `gcc`。

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
- [wireless_v1 模块说明](code/wireless_v1/README.md)
- [line_follow_v1 仿真测试说明](TDPS-Simulator/sim_tests/line_follow_v1/README.md)

### 设计文档

- [总体设计](docs/basic_design.md)
- [巡线子系统设计](docs/line_following.md)
- [无线通信设计](docs/wireless_comm.md)

## 目录结构

```text
TDPS/
├── code/
│   ├── line_follow_v1/           # Task 1：巡线固件实现
│   │   ├── Inc/                  # 头文件
│   │   └── Src/                  # 源文件
│   └── wireless_v1/              # Task 2：无线通信实现
│       ├── Inc/                  # 头文件（配置、平台抽象、LoRa 驱动、协议、应用）
│       └── Src/                  # 源文件（含 PC 桩与 STM32 HAL 实现）
├── docs/                         # 设计与回归文档
├── TDPS-Simulator/
│   ├── scripts/                  # 仿真测试 CLI 与构建脚本
│   ├── sim_tests/                # 离线仿真测试代码（common + suite）
│   └── artifacts/                # 仿真产物（bin / reports / logs）
└── slides/                       # 课题展示材料与手册
```

## 板级接入（STM32CubeIDE）

### 巡线模块

实机烧录请直接看：[`code/line_follow_v1/README.md`](code/line_follow_v1/README.md)

核心要求：
- 编译宏启用 `LF_USE_STM32F4_HAL_PORT`
- 按接线修改 `Inc/lf_port_stm32f4_hal.h`
- 在工程中提供 `g_lf_sensor_dma_buffer`、`LF_Port_SystemClock_Config`、`LF_Port_Peripheral_Init`

### 无线模块

详见：[`code/wireless_v1/README.md`](code/wireless_v1/README.md)

核心要求：
- 编译宏启用 `WL_USE_STM32F1_HAL_PORT`
- 按接线修改 `Inc/wl_port_stm32f1.h`
- 工程包含 STM32F1xx HAL 库

## 生成物

以下产物统一输出到 `TDPS-Simulator/artifacts/line_follow_v1/`：

- `bin/lf_autotest_runner`
- `reports/`
- `logs/*.log`
