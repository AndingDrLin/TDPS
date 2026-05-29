# Standalone LoRa Module Test

独立 Keil 工程路径：`MDK-ARM/lora_module_test.uvprojx`。

## 硬件与引脚

目标芯片：STM32F407VETx。

LoRa 模块：EWM22A-900BWL22S。

| 功能 | MCU 引脚 | 说明 |
|---|---|---|
| UART TX | PC12 / UART5_TX | MCU 发往 LoRa RXD |
| UART RX | PD2 / UART5_RX | MCU 接收 LoRa TXD |
| LINK | PD0 | 输入 |
| WAKE | PD1 | 输出，默认低电平 |
| AUX | PE0 | 输入，高电平表示模块空闲 |
| RST | PE1 | 输出，低电平复位 |

## 默认 LoRa 参数

- 模块地址：`0x0001`
- 信道：`10`
- 手册公式：900 系列实际频率 = `850.125 + CH × 1MHz`，因此 `CH=10` 对应 `860.125MHz`；若需要约 900MHz，应重新选择信道
- 空中速率：`AT+RATE=0`，2.4Kbps
- 网络 ID：`0x00`
- 封包长度：`AT+PACKET=0`，240 字节
- 传输模式：`AT+TRANS=1`，Fixed Mode
- LoRa Key：`AT+KEY=0`，不加密
- 测试发送目标：`0xFFFF` 广播地址，目标信道为 `10`
- 测试 payload：`Team 15 PTSD\r\n`

## Keil Watch 操作

烧录后查看全局变量：`g_lora_test`。

`g_lora_test.command` 可写入以下命令：

| 值 | 动作 |
|---|---|
| 1 | 刷新 AUX/LINK/WAKE/RST 状态 |
| 2 | 复位 LoRa 模块 |
| 3 | 重新执行 AT 参数配置 |
| 4 | 发送 Fixed Mode 测试帧 |
| 5 | WAKE 置低 |
| 6 | WAKE 置高 |

`g_lora_test.last_command` 和 `g_lora_test.last_response` 可查看最近一条 AT 指令及响应。
`g_lora_test.last_payload` 可查看最近一次发送的明文 payload。

## 预期结果

1. `g_lora_test.magic` 应为 `0x4C4F5241`。
2. `g_lora_test.uptime_ms` 持续增长。
3. 执行配置后 `g_lora_test.at_step` 到达 `15`，`last_status` 为 `0`。
4. `at_ok_count` 随 AT 成功响应增加。
5. 发送测试帧后 `tx_count` 增加。

## 故障排查

- AUX 一直为低：检查模块供电、RST、WAKE 和 AUX 接线。
- AT 超时：检查 UART5 PC12/PD2 是否与模块 RXD/TXD 正确交叉连接。
- Fixed Mode 收不到数据：确认接收端模块同信道、同网络 ID、未加密；本工程发送使用广播地址 `0xFFFF`。
