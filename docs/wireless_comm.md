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
  - `wl_lora`: AT 配置 + 异步发送队列 + 超时重试 + 可选 ACK
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
- LoRa 收发通过 `WL_App_Tick()` 非阻塞推进，不阻塞巡线控制闭环。

## 异步发送与重试

- 检查点消息使用 `WL_LoRa_EnqueueString()` 入队。
- `WL_LoRa_Tick()` 负责：
  - 检查发送节流与 AUX 就绪状态
  - 发送事务超时判定（`WL_TX_TIMEOUT_MS`）
  - ACK 等待与超时（`WL_ACK_TIMEOUT_MS`，可选）
  - 失败重试（`WL_TX_RETRY_MAX`）
- 链路状态通过 `WL_LoRa_GetLinkStatus()` 查询：队列深度、重试次数、成功/失败计数。

## 自动化测试

统一入口：

```bash
mkdir -p firmware/build/gcc

gcc -Ifirmware/Inc -Ifirmware/common -Ifirmware/platform \
    firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    firmware/test/test_wl_stub.c -o firmware/build/gcc/wl_test
./firmware/build/gcc/wl_test
```

附加巡线回归依赖 `TDPS-Simulator` 子模块：

```bash
bash TDPS-Simulator/scripts/line_follow_cli.sh quick
bash TDPS-Simulator/scripts/run_line_follow_autotest.sh
```
