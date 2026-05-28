# TDPS_BoardTest 板级测试程序使用指南

本工程用于测试 TDPS PCB 的基础硬件功能，包括 MCU、ST-Link 下载调试、雷达、LoRa、巡线传感器、编码器和电机驱动。工程路径：

```text
TDPS/tests/board/tdps_board_test
```

本目录现在只保留板级测试业务源码和测试说明；原 Keil/CubeMX 工程附加文件和 HAL/启动脚手架已从非 firmware 目录移除。

当前测试设计支持两种方式：

1. **只用 ST-Link/SWD + Keil Watch 窗口测试**：推荐当前使用，因为不需要 TTL 转串口。
2. **USART6 串口 CLI 测试**：以后有 USB-TTL 或串口工具时可用。

## 1. 硬件连接

### 1.1 必须连接

使用 ST-Link/SWD 下载和调试时，至少连接：

| ST-Link | TDPS PCB |
|---|---|
| SWDIO | SWDIO / PA13 |
| SWCLK | SWCLK / PA14 |
| GND | GND |
| 3.3V reference | V_3V3_MCU / 3V3 |
| NRST，可选但推荐 | NRST |

注意：ST-Link 的 3.3V 通常只作为目标电压参考，不建议用它给整块 PCB 供电。PCB 应使用正常电源输入，并确认 5V、3.3V 稳定。

### 1.2 电机测试前要求

电机测试前必须满足：

- 轮子架空，避免小车突然移动。
- 使用限流电源。
- 确认电机和驱动芯片没有明显发热。
- 先用 5% 占空比短时间测试。
- 任意异常时立即执行 `command = 12` 停止电机，或按复位键/断电。

## 2. 原理图引脚对应关系

测试程序按原理图引脚定义配置：

| 模块 | 信号 | MCU 引脚 | 外设 / 模式 |
|---|---|---|---|
| 调试串口 | MONI_TX / MONI_RX | PC6 / PC7 | USART6，115200 8N1 |
| 雷达 | PB10_RADAR_TX / PB11_RADAR_RX | PB10 / PB11 | USART3，115200 8N1 |
| 雷达 GPIO | PE15_RADAR_GPIO | PE15 | GPIO input |
| LoRa UART | PC12_LORA_TX / PD2_LORA_RX | PC12 / PD2 | UART5，115200 8N1 |
| LoRa LINK | PD0_LORA_LINK | PD0 | GPIO input |
| LoRa WAKE | PD1_LORA_WAKE | PD1 | GPIO output |
| LoRa AUX | PE0_LORA_AUX | PE0 | GPIO input |
| LoRa RST | PE1_LORA_RST | PE1 | GPIO output |
| 巡线 UART | PA2_LINE_TX / PA3_LINE_RX | PA2 / PA3 | USART2，115200 8N1 |
| 双路巡线 IO | PB0_LINE2_SIG1 / PB1_LINE2_SIG2 | PB0 / PB1 | GPIO input |
| 电机 A | MOT_A_PWM / MOT_A_DIR | PC8 / PC9 | TIM8_CH3 PWM + GPIO |
| 电机 B | MOT_B_PWM / MOT_B_DIR | PB8 / PB9 | TIM10_CH1 PWM + GPIO |
| 编码器 A | ENC_A_A / ENC_A_B | PB4 / PB5 | TIM3 encoder |
| 编码器 B | ENC_B_A / ENC_B_B | PB6 / PB7 | TIM4 encoder |

## 3. Keil5 编译和烧录

原 Keil5 工程文件已从非 firmware 目录清理。后续如需要重新上板运行本测试，应在 `firmware/` 内或新的受控测试工程中接入这些 `board_*` 模块。

## 4. 推荐方式：只用 ST-Link + Keil Watch 测试

本工程定义了一个全局变量：

```c
g_board_swd
```

它可以直接在 Keil Watch 窗口中查看和修改。

### 4.1 打开 Watch 窗口

1. 下载程序后进入 Keil Debug 模式。
2. 打开：

   ```text
   View -> Watch Windows -> Watch 1
   ```

3. 在 Watch 里添加变量：

   ```text
   g_board_swd
   ```

4. 展开 `g_board_swd`，可以看到所有状态和控制字段。

### 4.2 g_board_swd 结构说明

关键字段：

| 字段 | 用途 |
|---|---|
| `magic` | 固定值，应为 `0x54445053`，表示测试程序正在运行 |
| `command` | 写入命令编号，主循环执行后会自动清回 0 |
| `motor_a_duty` | 电机 A 目标占空比，单位百分比，可正可负 |
| `motor_b_duty` | 电机 B 目标占空比，单位百分比，可正可负 |
| `motor_duration_ms` | 电机运行时间，单位 ms |
| `lora_wake_level` | LoRa WAKE 输出电平，0 或 1 |
| `last_command` | 最近执行过的命令 |
| `command_count` | 已执行命令次数 |
| `error_count` | 命令错误次数 |
| `uptime_ms` | 系统运行时间 |
| `sysclk_hz` | 当前系统时钟 |
| `motor_locked` | 电机锁定状态，1 表示锁定 |
| `motor_active` | 电机是否正在运行 |
| `encoder_a` / `encoder_b` | 编码器计数 |
| `radar_gpio` | 雷达 GPIO PE15 当前电平 |
| `radar_rx_count` | 雷达 UART 收到的字节数 |
| `radar_target_state` | 雷达解析出的目标状态 |
| `radar_distance_cm` | 雷达解析出的距离，单位 cm |
| `lora_aux` / `lora_link` / `lora_wake` / `lora_rst` | LoRa GPIO 状态 |
| `lora_rx_count` | LoRa UART 收到的字节数 |
| `line_sig1` / `line_sig2` | 双路巡线 IO 状态 |
| `line_rx_count` | 巡线 UART 收到的字节数 |

### 4.3 command 命令表

向 `g_board_swd.command` 写入以下数值即可触发测试：

| command | 功能 |
|---:|---|
| 0 | 无命令 |
| 1 | 刷新状态 |
| 10 | 电机解锁 |
| 11 | 电机锁定并停止 |
| 12 | 电机立即停止 |
| 13 | 按 `motor_a_duty` / `motor_b_duty` / `motor_duration_ms` 运行电机 |
| 20 | 编码器清零 |
| 30 | LoRa 复位 |
| 31 | 设置 LoRa WAKE，先设置 `lora_wake_level` |
| 40 | 被动自检，不启动电机 |
| 41 | 电机安全自检，需要先解锁 |

主循环执行命令后会自动把 `command` 清回 0。

## 5. ST-Link 测试步骤

### 5.1 MCU 基础测试

进入 Debug 后，在 Watch 中查看：

```text
g_board_swd.magic
g_board_swd.uptime_ms
g_board_swd.sysclk_hz
```

预期：

- `magic = 0x54445053`
- `uptime_ms` 持续增加
- `sysclk_hz` 有合理数值，当前默认使用 HSI 16 MHz

如果 `uptime_ms` 不变化，说明程序没有运行，检查是否停在断点、HardFault 或复位状态。

### 5.2 被动自检

设置：

```text
g_board_swd.command = 40
```

然后观察：

```text
g_board_swd.command
g_board_swd.last_command
g_board_swd.command_count
g_board_swd.error_count
```

预期：

- `command` 自动回到 0
- `last_command = 40`
- `command_count` 增加
- `error_count` 不增加

被动自检不会让电机转动。

### 5.3 GPIO 状态测试

设置：

```text
g_board_swd.command = 1
```

观察：

```text
g_board_swd.radar_gpio
g_board_swd.lora_aux
g_board_swd.lora_link
g_board_swd.lora_wake
g_board_swd.lora_rst
g_board_swd.line_sig1
g_board_swd.line_sig2
```

测试方法：

- 插拔或遮挡雷达，观察 `radar_gpio` 是否变化。
- 操作 LoRa WAKE/RST 后观察对应状态。
- 改变巡线传感器黑白状态，观察 `line_sig1`、`line_sig2`。

### 5.4 编码器测试

先清零：

```text
g_board_swd.command = 20
```

手动转动车轮后刷新：

```text
g_board_swd.command = 1
```

观察：

```text
g_board_swd.encoder_a
g_board_swd.encoder_b
```

预期：

- 手动转 A 电机对应轮子时，`encoder_a` 变化。
- 手动转 B 电机对应轮子时，`encoder_b` 变化。
- 如果左右反了，说明编码器线序或电机定义需要在后续控制程序中修正。

### 5.5 电机 A 低速测试

必须先架空轮子。

1. 解锁电机：

   ```text
   g_board_swd.command = 10
   ```

2. 设置 A 电机低速运行参数：

   ```text
   g_board_swd.motor_a_duty = 5
   g_board_swd.motor_b_duty = 0
   g_board_swd.motor_duration_ms = 300
   ```

3. 启动：

   ```text
   g_board_swd.command = 13
   ```

4. 观察：

   ```text
   g_board_swd.motor_active
   g_board_swd.encoder_a
   g_board_swd.encoder_b
   ```

预期：

- A 电机短暂转动约 300 ms。
- `motor_active` 运行后自动回到 0。
- `encoder_a` 变化。

如果方向不对，后续可以在方向极性宏中调整。

### 5.6 电机 B 低速测试

设置：

```text
g_board_swd.motor_a_duty = 0
g_board_swd.motor_b_duty = 5
g_board_swd.motor_duration_ms = 300
g_board_swd.command = 13
```

观察：

```text
g_board_swd.encoder_a
g_board_swd.encoder_b
```

预期：

- B 电机短暂转动。
- `encoder_b` 变化。

### 5.7 电机反向测试

A 电机反向：

```text
g_board_swd.motor_a_duty = -5
g_board_swd.motor_b_duty = 0
g_board_swd.motor_duration_ms = 300
g_board_swd.command = 13
```

B 电机反向：

```text
g_board_swd.motor_a_duty = 0
g_board_swd.motor_b_duty = -5
g_board_swd.motor_duration_ms = 300
g_board_swd.command = 13
```

测试完锁定：

```text
g_board_swd.command = 11
```

任意时刻紧急停止：

```text
g_board_swd.command = 12
```

### 5.8 雷达测试

雷达连接并上电后，刷新状态：

```text
g_board_swd.command = 1
```

观察：

```text
g_board_swd.radar_gpio
g_board_swd.radar_rx_count
g_board_swd.radar_error_count
g_board_swd.radar_target_state
g_board_swd.radar_distance_cm
g_board_swd.radar_last_frame_ms
```

预期：

- 如果 UART 正常，`radar_rx_count` 会增加。
- 如果雷达检测到目标，`radar_target_state` 和 `radar_distance_cm` 会变化。
- 人靠近或移动时，`radar_gpio` 可能变化，具体取决于雷达模块配置。

如果 `radar_rx_count` 一直为 0：

1. 检查雷达供电。
2. 检查 PB10/PB11 是否接反。
3. 检查雷达波特率是否为 115200。
4. 检查雷达 TX 是否输出波形。

### 5.9 巡线传感器测试

刷新：

```text
g_board_swd.command = 1
```

观察：

```text
g_board_swd.line_sig1
g_board_swd.line_sig2
g_board_swd.line_rx_count
g_board_swd.line_error_count
g_board_swd.line_last_rx_ms
```

测试方法：

- 用黑线/白底切换巡线探头状态。
- 如果使用双路 IO，`line_sig1` / `line_sig2` 应变化。
- 如果巡线模块 UART 模式工作，`line_rx_count` 应增加。

如果 `line_rx_count` 不增加：

1. 检查巡线模块是否处于 UART 模式。
2. 检查 PA2/PA3 TX/RX 是否接反。
3. 检查波特率是否为 115200。

### 5.10 LoRa 测试

观察基础状态：

```text
g_board_swd.command = 1
```

查看：

```text
g_board_swd.lora_aux
g_board_swd.lora_link
g_board_swd.lora_wake
g_board_swd.lora_rst
g_board_swd.lora_rx_count
g_board_swd.lora_error_count
```

复位 LoRa：

```text
g_board_swd.command = 30
```

设置 WAKE：

```text
g_board_swd.lora_wake_level = 0
g_board_swd.command = 31
```

或：

```text
g_board_swd.lora_wake_level = 1
g_board_swd.command = 31
```

如果有对端 LoRa 模块发送数据，`lora_rx_count` 应增加。

注意：EWM22A 模块供电必须为 3.0V 到 3.6V，不能接 5V。

## 6. 可选方式：USART6 串口 CLI

如果以后有 USB-TTL 转接线，可以连接：

| TDPS PCB | USB-TTL |
|---|---|
| PC6 / MONI_TX | RX |
| PC7 / MONI_RX | TX |
| GND | GND |

串口参数：

```text
115200 8N1，无流控
```

### 6.1 基础命令

```text
help
version
status
pins
uptime
reboot
```

### 6.2 GPIO

```text
gpio read all
```

### 6.3 雷达

```text
radar status
radar gpio
radar raw on
radar raw off
radar parse on
radar parse off
```

### 6.4 LoRa

```text
lora status
lora reset
lora wake 0
lora wake 1
lora raw on
lora raw off
lora send hello
```

### 6.5 巡线

```text
line status
line io
line raw on
line raw off
line send hello
```

### 6.6 编码器

```text
enc read
enc zero
enc watch 1000
```

注意：`enc watch` 会短时间阻塞主循环，因此程序禁止在电机运行时执行该命令。

### 6.7 电机

```text
motor status
motor unlock
motor lock
motor stop
motor max 20
motor a 5 300
motor b 5 300
motor both 5 5 300
stop
```

规则：

- 默认上电锁定。
- 必须先 `motor unlock`。
- 占空比默认最大 20%。
- 每次电机命令必须带时间。
- 最长运行 1000 ms。
- 到时自动停止。

## 7. 推荐完整测试顺序

建议按以下顺序测试，不要一上来就测电机：

1. 上电前检查短路：`V_BAT`、`V_5V`、`V_3V3_MCU` 到 GND。
2. 限流上电，测量 `V_5V`、`V_3V3_MCU`、`V_3V3_SYS`。
3. 用 ST-Link 下载程序。
4. Keil Watch 查看 `g_board_swd.magic` 和 `uptime_ms`。
5. 执行 `command = 40` 被动自检。
6. 观察 LoRa、雷达、巡线 GPIO。
7. 手动转动车轮测试编码器。
8. 架空轮子后，低占空比测试电机。
9. 测雷达 UART/GPIO。
10. 测巡线 IO/UART。
11. 测 LoRa GPIO/UART。
12. 所有单项通过后，再考虑后续控制算法。

## 8. 常见问题

### 8.1 Keil Watch 找不到 g_board_swd

检查：

- 是否已经 Rebuild All。
- 是否下载的是当前工程。
- 是否进入 Debug 模式。
- 优化等级过高时，变量仍通过 `__attribute__((used))` 保留；如果仍看不到，可以在 Keil 中临时降低优化等级并重新编译。

### 8.2 command 写入后没有执行

检查：

- 程序是否暂停在断点。如果暂停，主循环不会执行命令。
- 写入后是否恢复运行。
- `uptime_ms` 是否增加。
- `command_count` 是否增加。

### 8.3 电机不转

检查：

- `motor_locked` 是否为 0。
- `motor_duration_ms` 是否在 1 到 1000 之间。
- duty 是否超过默认 20%。
- 电机供电是否正常。
- DRV8874 是否有故障或未使能。
- PWM 引脚 PC8/PB8 是否有波形。
- DIR 引脚 PC9/PB9 是否变化。

### 8.4 编码器不计数

检查：

- 编码器供电是否正常。
- PB4/PB5、PB6/PB7 是否有脉冲。
- TIM3/TIM4 是否正确进入 encoder mode。
- 编码器 A/B 相是否接反。

### 8.5 雷达没有数据

检查：

- 雷达供电。
- PB10/PB11 TX/RX 方向。
- 波特率 115200。
- 雷达是否已经上电启动。
- `radar_gpio` 是否有变化。

### 8.6 巡线没有数据

检查：

- 模块实际工作模式是 UART 还是 IO。
- UART 模式下 PA2/PA3 是否接反。
- IO 模式下 PB0/PB1 是否有电平变化。
- 模块供电是否符合要求。

### 8.7 LoRa 没有响应

检查：

- 供电是否为 3.3V，不能为 5V。
- RST 是否保持高电平。
- WAKE 有效电平是否正确。
- PC12/PD2 TX/RX 方向。
- 对端模块是否同频率、同地址、同信道。

## 9. 文件结构

```text
tdps_board_test/
├── Inc/
│   ├── board_cli.h
│   ├── board_encoder.h
│   ├── board_line.h
│   ├── board_lora.h
│   ├── board_motor.h
│   ├── board_pins.h
│   ├── board_radar.h
│   ├── board_swd.h
│   ├── board_test.h
│   └── board_uart_router.h
├── Src/
│   ├── board_cli.c
│   ├── board_encoder.c
│   ├── board_line.c
│   ├── board_lora.c
│   ├── board_motor.c
│   ├── board_radar.c
│   ├── board_swd.c
│   ├── board_test.c
│   └── board_uart_router.c
├── SWD_TEST_GUIDE.txt
└── README.md
```

## 10. 安全提醒

- 首次上电必须限流。
- 电机测试必须架空轮子。
- 默认电机锁定，不要绕过该保护。
- LoRa 模块只能接 3.3V。
- 如果任何芯片发热、复位、冒烟、异常电流，立即断电。
- 在所有单模块测试通过前，不要运行完整小车控制逻辑。
