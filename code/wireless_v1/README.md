# wireless_v1

Task 2 无线通信实现：小车经过检查点时发送 LoRa 报文，并在主循环中以异步队列非阻塞推进发送事务。

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

异步 LoRa 服务接口：

- `WL_LoRa_ServiceInit()`
- `WL_LoRa_EnqueueString()`
- `WL_LoRa_Tick()`
- `WL_LoRa_SetAckEnabled()`
- `WL_LoRa_GetLinkStatus()`

巡线侧通过 `LF_App_NotifyCheckpoint(checkpoint_id)` 触发无线发送。

## 发送策略

- 检查点事件入队，不在应用层阻塞等待发送完成。
- `WL_LoRa_Tick()` 执行发送、超时检查、重试与 ACK 解析。
- ACK 默认关闭（`WL_ACK_ENABLE=false`），联调网关后可开启。
- 关键参数集中在 `wl_config.h/.c`：
	- `WL_TX_QUEUE_SIZE`
	- `WL_TX_TIMEOUT_MS`
	- `WL_TX_RETRY_MAX`
	- `WL_ACK_ENABLE`
	- `WL_ACK_TIMEOUT_MS`
	- `WL_STATUS_REPORT_PERIOD_MS`

## 本地自动化测试

```bash
bash TDPS-Simulator/scripts/run_wireless_autotest.sh
```

包含：
- `wl_async_autotest`: 队列发送、ACK 成功、超时重试、AUX busy 超时
- `lf_radar_lora_integration_autotest`: 巡线 + 雷达 + LoRa 联动桩测试

## STM32 接入

- 定义 `WL_USE_STM32F1_HAL_PORT`
- 按实际接线修改 `Inc/wl_port_stm32f1.h`
- 工程中加入 `Src`（排除 `wl_platform_stub.c`）
