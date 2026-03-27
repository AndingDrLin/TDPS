# wireless_v1

Task 2 无线通信实现：小车经过检查点时发送 LoRa 报文。

## 协议

报文格式：

```text
TEAM=15,NAME=TDPS,CP=21,TIME=5000\n
```

- `TEAM`: 队伍编号
- `NAME`: 队伍名称
- `CP`: 检查点 ID
- `TIME`: 比赛开始后毫秒数

## 接口

- `WL_App_Init()`
- `WL_App_StartRace()`
- `WL_App_NotifyCheckpoint(checkpoint_id)`
- `WL_App_StopRace()`

巡线侧通过 `LF_App_NotifyCheckpoint(checkpoint_id)` 触发无线发送。

## 本地自动化测试

```bash
bash TDPS-Simulator/scripts/run_wireless_autotest.sh
```

包含：
- `wl_stub_autotest`: 无线模块桩测试
- `lf_wireless_integration_autotest`: 巡线+无线集成测试

## STM32 接入

- 定义 `WL_USE_STM32F1_HAL_PORT`
- 按实际接线修改 `Inc/wl_port_stm32f1.h`
- 工程中加入 `Src`（排除 `wl_platform_stub.c`）
