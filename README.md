# TDPS Workspace Guide

本仓库现在以 `TDPS/` 作为主项目目录，集中管理固件、仿真、测试、文档、报告脚本和 agent 上下文。

## Top-level layout

- `TDPS/firmware/`: STM32F407VET6 主固件工程。
- `TDPS/simulator/`: 离线仿真和回归测试代码，来自原 `TDPS-Simulator`。
- `TDPS/tests/`: 板级测试和独立外设测试。
- `TDPS/agent_docs/`: 给 agent/Claude 快速读取的任务要求、硬件信息、当前进度和工作注意事项。
- `TDPS/docs/`: 手写设计文档、巡线、无线、调参和测试说明。
- `TDPS/docs_reference/`: 本地硬件手册、课程 slides 和厂商资料索引；大型二进制资料不提交到 Git。
- `TDPS/reports/`: 报告生成脚本和实验记录项目。
- `TDPS/archive/experiments/`: 临时或历史实验代码归档。

## Quick start

1. 阅读 `TDPS/agent_docs/00_project_brief.md` 了解当前项目目标和目录。
2. 阅读 `TDPS/agent_docs/01_hardware_reference.md` 查常用硬件与引脚信息。
3. 阅读 `TDPS/agent_docs/02_current_progress.md` 查当前进度和风险。
4. 固件离线测试见 `TDPS/README.md`。
5. 报告脚本位于 `TDPS/reports/mid_reports/` 和 `TDPS/reports/notebook/`。

## Naming conventions

- 目录和新管理文件优先使用 `snake_case`。
- 厂商资料内部文件名保持原样，降低路径破坏风险。
- 生成产物集中在 `firmware/build/`、`simulator/artifacts/`、`reports/*/outputs/`，默认不提交。
