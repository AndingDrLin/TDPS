# 无线通信设计（Task 2）

## 目标

在巡线过程中，当外部识别到检查点事件时，发送 LoRa 报文：

```text
TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<ms>\n
```

当前配置：`TEAM=15`，`NAME=TDPS`。

## 实现结构

- `wireless_v1`：
  - `wl_app`: 状态机（READY/RUNNING/FINISHED）
  - `wl_protocol`: 报文打包
  - `wl_lora`: AT 配置 + 透传发送
  - `wl_platform_*`: STM32 实现 / PC 桩实现
- `line_follow_v1`：
  - `wireless_hooks.c`: 负责初始化无线、启动比赛计时、转发检查点事件
  - `LF_App_NotifyCheckpoint(checkpoint_id)`: 统一对外触发入口

## 集成链路

1. 系统启动：`Wireless_Hooks_Init()` -> `WL_App_Init()`
2. 标定完成：`LF_Hook_OnCalibrationComplete(true)` -> `WL_App_StartRace()`
3. 经过检查点：`LF_App_NotifyCheckpoint(id)` -> `LF_Hook_OnReservedCheckpoint(id)` -> `WL_App_NotifyCheckpoint(id)`

约束：
- 仅在 `RUNNING` 状态发送检查点报文。
- 首包不节流，后续发送间隔 `>= WL_TX_MIN_INTERVAL_MS`。

## 自动化测试

统一入口：

```bash
bash TDPS-Simulator/scripts/run_wireless_autotest.sh
```

包含：
- `wl_stub_autotest`：初始化、报文内容、节流、停止状态
- `lf_wireless_integration_autotest`：巡线检查点触发链路

附加回归：

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick
```
