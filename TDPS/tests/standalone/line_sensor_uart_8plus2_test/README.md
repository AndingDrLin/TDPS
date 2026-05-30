# line_sensor_uart_8plus2_test

8 路 UART 巡线模块 + 2 路辅助巡线 GPIO 独立测试工程，用于只通过 Keil 5、STLink 和 Debug Watch 验证传感器与 STM32F407 的配合。

本工程不依赖串口打印、不驱动电机、不启用雷达/LoRa；所有状态通过 Keil Watch 观察。

## Keil 工程

直接打开：

```text
TDPS/tests/standalone/line_sensor_uart_8plus2_test/MDK-ARM/line_sensor_uart_8plus2_test.uvprojx
```

目标 MCU：`STM32F407VETx`，使用 Keil 5 / ARMCC 5。

## 接线

### 8 路巡线模块 UART

| 8 路巡线模块 | STM32F407 | 说明 |
|---|---|---|
| RX | PA2 / USART2_TX | MCU 向模块发送命令 |
| TX | PA3 / USART2_RX | MCU 接收模块数据 |
| 5V | 5V | 模块供电 |
| GND | GND | 必须与 MCU 共地 |

串口参数：`115200 8N1`。

### 2 路辅助巡线 GPIO

| 信号 | STM32F407 | Watch 字段 |
|---|---|---|
| SIG1 | PB0 | `g_line_watch.aux_active[0]` |
| SIG2 | PB1 | `g_line_watch.aux_active[1]` |

默认 `LINE_AUX_ACTIVE_HIGH=0`，即低电平视为检测到黑线。如果实物模块是高电平有效，修改 `Core/Inc/main.h` 中的 `LINE_AUX_ACTIVE_HIGH`。

## UART 协议

根据 `TDPS/docs_reference/八路巡线红外传感器`，模块上电默认不发送数据，主控需要先发送命令。

本工程上电约 1 秒后发送：

```text
$0,1,1#
```

含义：同时请求模拟型数据和数字型数据。

模块返回示例：

```text
$A,x1:4096,x2:4096,x3:4096,x4:4096,x5:4096,x6:4096,x7:4096,x8:4096#
$D,x1:0,x2:0,x3:0,x4:0,x5:0,x6:0,x7:0,x8:0#
```

- `$A` 模拟值进入 firmware 巡线算法链路：raw -> norm -> filtered -> position -> PID。
- `$D` 数字值只用于 Watch 中确认每路 0/1 和极性。

## Keil Watch

Debug 时在 Watch 窗口添加：

```c
g_line_watch
g_line_frame
g_line_pid
g_lf_config
huart2
```

常用字段：

- `rx_byte_count`：USART2 收到的字节数，持续增长说明 PA2/PA3 通信有数据。
- `analog_frame_count`：成功解析 `$A` 模拟帧的次数。
- `digital_frame_count`：成功解析 `$D` 数字帧的次数。
- `parse_error_count`：ASCII 帧解析错误次数。
- `uart_error_count`：HAL UART 错误回调次数。
- `last_ascii_frame`：最近解析成功的一帧原始 ASCII 文本。
- `uart_analog[8]`：X1~X8 的模拟值。
- `uart_digital[8]`：X1~X8 的数字值。
- `main_raw[8]`：实际送入 `LF_Sensor_ReadFrame()` 的 raw 值。
- `norm[8]` / `filtered_u16[8]`：firmware 巡线算法处理后的 8 路强度。
- `position`：巡线位置，左侧为负、右侧为正。
- `line_detected`：是否检测到线。
- `aux_active[2]` / `aux_active_mask`：PB0/PB1 两路辅助巡线状态。
- `motor_left_raw` / `motor_right_raw`：PID 计算出的理论左右轮命令。
- `motor_left_applied` / `motor_right_applied`：经过底盘限幅后捕获的虚拟电机命令，不会实际输出 PWM。
- `fault_flags`：bit0 表示启动命令发送失败，bit1 表示启动/重启中断接收失败。

## 测试步骤

1. 先按模块文档完成 8 路模块自身黑线/白底校准。
2. 按上表接线，确认 5V/GND 和 PA2/PA3 没接反。
3. Keil 打开本工程，Build Target。
4. 使用 STLink 下载并进入 Debug。
5. Watch 添加 `g_line_watch`、`g_line_frame`、`g_line_pid`、`g_lf_config`、`huart2`。
6. 运行程序，确认 `rx_byte_count` 增长，且 `analog_frame_count` / `digital_frame_count` 至少一个增长。
7. 逐个遮挡 X1~X8，观察 `uart_analog[]`、`uart_digital[]`、`filtered_u16[]` 和 `peak_index`。
8. 让黑线从左向右移动，确认 `position` 从负值逐渐变为正值。
9. 遮挡 PB0/PB1 两路辅助传感器，确认只改变 `aux_*` 字段，不影响 8 路 `position`。

## 常见问题

- `rx_byte_count` 不增长：检查 PA2/PA3 是否接反、GND 是否共地、模块是否上电。
- `rx_byte_count` 增长但帧计数不增长：检查模块是否使用文档中的 `$A/$D` ASCII 协议，或查看 `last_rx_byte`/`parse_error_count`。
- 模块不发送数据：确认启动命令 `$0,1,1#` 已发送，`fault_flags` bit0 没有置位。
- `position` 左右反了：优先检查模块 X1~X8 的安装方向；必要时调整接线或权重，不要先改 PID。
- 黑线强度方向反了：在 Watch 中比较黑线和白底下的 `uart_analog[]`；如果黑线数值更小，将 `g_lf_config.sensor_invert_polarity` 设为 `true` 后再测。
- PB0/PB1 辅助极性反了：修改 `Core/Inc/main.h` 中的 `LINE_AUX_ACTIVE_HIGH`。
- Keil 报 `LD2410S_*` 或 `huart3` 未定义：说明工程误引用了旧 radar 文件，应确认 uvprojx 中没有 `ld2410s.c`、`USART3_IRQHandler`。

## 复用 firmware 逻辑

本工程复制并复用了 firmware 的以下核心逻辑：

- `lf_config.*`
- `lf_sensor.*`
- `lf_control.*`
- `lf_chassis.*`
- `lf_platform.h`
- `clamp.h`

standalone 专用平台层在 `Core/Src/lf_platform_standalone.c` 中，只负责把 UART 模拟帧提供给算法、捕获虚拟电机命令和提供 HAL tick。
