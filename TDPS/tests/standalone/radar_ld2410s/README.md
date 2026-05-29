# radar_ld2410s

HLK-LD2410S 雷达独立测试工程，用于整车装配后通过 Keil 5 + STLink 观察雷达状态。

## Keil 工程

直接打开：

```text
TDPS/tests/standalone/radar_ld2410s/MDK-ARM/radar_ld2410s.uvprojx
```

工程目标 MCU：STM32F407VETx，使用 ARMCC 5。

## 接线

- 雷达 VCC -> 3.3 V
- 雷达 GND -> MCU GND
- 雷达 OT1 / UART_TX -> PB11 / MCU USART3_RX
- 雷达 RX / UART_RX -> PB10 / MCU USART3_TX
- 雷达 OT2 / GPIO output -> PB0
- 调试串口 USART2_TX -> PA2，USART2_RX -> PA3，115200 8N1

## Keil Watch

本测试不依赖板载 LED，所有状态都通过 Keil Watch 观察。

调试时在 Watch 窗口添加：

```c
g_radar_watch
```

常用字段：

- `raw_rx_bytes`: USART3 原始接收字节计数，持续增长说明串口有数据。
- `frame_count`: 成功解析的雷达帧计数，持续增长说明协议解析正常。
- `uart_fresh`: 最近 300 ms 内是否有有效帧。
- `uart_target`: UART 数据是否检测到目标。
- `gpio_target`: PB0 GPIO 是否检测到目标。
- `raw_distance_cm`: 雷达原始距离。
- `filtered_distance_cm`: 滤波后的距离。
- `left_obstacle_present`: 是否判定左侧障碍存在。
- `path_turn_right`: 是否输出模拟右转命令。
- `parse_error_count`: 解析错误计数。
- `rx_overflow_count`: 接收环形缓冲区溢出计数。
- `last_rx_status`: 最近一次启动串口中断接收的 HAL 状态。

## 旧文件说明

根目录下的 `main.c`、`main.h`、`ld2410s.c`、`ld2410s.h` 保留为便于直接查看/拷贝的副本；Keil 工程实际编译 `Core/Src` 和 `Core/Inc` 下的文件。

`radar_test_main.c` 是早期可合并进 CubeMX 工程的参考模板。
