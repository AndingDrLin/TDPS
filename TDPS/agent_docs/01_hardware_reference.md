# 常用硬件参考

## 主控与底盘

| 项目 | 当前记录 |
|---|---|
| MCU | STM32F407VET6 |
| 控制方式 | 差速底盘，左右独立驱动 |
| 底盘资料 | `TDPS/docs_reference/Wheeltec L150小车底盘/` |
| 开发板资料 | `TDPS/docs_reference/STM32F407VET6开发板/` |

## 传感器与通信模块

| 模块 | 用途 | 当前记录 / 注意事项 |
|---|---|---|
| 8 路灰度巡线传感器 | 巡线输入 | 固件默认 `LF_SENSOR_COUNT=8`；支持 ADC 模拟量和数字 GPIO 模式 |
| EWM22A-900BWL22S LoRa | 检查点通信 | `TEAM=15`，`NAME=TDPS`；发送格式见 `TDPS/docs/wireless_comm.md` |
| HLK-LD2410S | 雷达避障 | 当前用于 WARN/BLOCK/CLEAR 状态和距离判断，仍需实机确认波特率、距离单位和状态语义 |
| DMG48270C043_04WN | 屏幕 | 本地资料在 `TDPS/docs_reference/` 和历史 monitor 实验资料中 |

## 电源与驱动

| 器件 | 用途 | 注意事项 |
|---|---|---|
| LM2596S | 降压电源 | 待补充实际输入/输出配置和 PCB 节点 |
| AMS1117 | 线性稳压 | 待补充对应 3.3V 支路和负载 |
| DRV8874 | 电机驱动 | 电机测试前必须架空轮子、限流上电、低占空比短时验证 |

## 板级测试关键引脚

来源：`TDPS/tests/board/tdps_board_test/README.md`

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

## 待补充

- 最终实物接线表和 PCB 版本对应关系。
- 电池、电机、轮径、编码器线数、实际电源树。
- 屏幕工程源文件和当前串口参数。
- 左右分叉避障所需的传感器安装方案或判定方法。
