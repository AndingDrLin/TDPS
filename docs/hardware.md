# Hardware Reference

## Main Controller & Chassis

| Item | Current Record |
|---|---|
| MCU | STM32F407VET6 |
| Drive | Differential chassis, independent left/right motor drive |
| Chassis docs | `docs_reference/Wheeltec L150小车底盘/` |
| Dev board docs | `docs_reference/STM32F407VET6开发板/` |

## Sensors & Communication Modules

| Module | Purpose | Notes |
|---|---|---|
| 8-channel IR line sensor | Line-following input | Default `LF_SENSOR_COUNT=8`; supports ADC analog and digital GPIO modes |
| EWM22A-900BWL22S LoRa | Checkpoint communication | `TEAM=15`, `NAME=TDPS`; format see `docs/wireless_comm.md` |
| HLK-LD2410S | Radar obstacle avoidance | Used for WARN/BLOCK/CLEAR state and distance; baud rate, distance unit, and state semantics need real-device confirmation |
| DMG48270C043_04WN | Display screen | Local docs in `docs_reference/` |

## Power & Motor Driver

| Device | Purpose | Notes |
|---|---|---|
| LM2596S | Buck converter | TBD: actual input/output config and PCB node |
| AMS1117 | LDO regulator | TBD: 3.3V rail and load |
| DRV8874 | Motor driver | Always raise wheels before motor test; use current-limited power and low duty cycle |

## Pin Mapping

Source: `TDPS/tests/board/tdps_board_test/README.md`

| Module | Signal | MCU Pin | Peripheral / Mode |
|---|---|---|---|
| Debug UART | MONI_TX / MONI_RX | PC6 / PC7 | USART6, 115200 8N1 |
| Radar | PB10_RADAR_TX / PB11_RADAR_RX | PB10 / PB11 | USART3, 115200 8N1 |
| Radar GPIO | PE15_RADAR_GPIO | PE15 | GPIO input |
| LoRa UART | PC12_LORA_TX / PD2_LORA_RX | PC12 / PD2 | UART5, 115200 8N1 |
| LoRa LINK | PD0_LORA_LINK | PD0 | GPIO input |
| LoRa WAKE | PD1_LORA_WAKE | PD1 | GPIO output |
| LoRa AUX | PE0_LORA_AUX | PE0 | GPIO input |
| LoRa RST | PE1_LORA_RST | PE1 | GPIO output |
| Line sensor UART | PA2_LINE_TX / PA3_LINE_RX | PA2 / PA3 | USART2, 115200 8N1 |
| Line sensor IO | PB0_LINE2_SIG1 / PB1_LINE2_SIG2 | PB0 / PB1 | GPIO input |
| Motor A | MOT_A_PWM / MOT_A_DIR | PC8 / PC9 | TIM8_CH3 PWM + GPIO |
| Motor B | MOT_B_PWM / MOT_B_DIR | PB8 / PB9 | TIM10_CH1 PWM + GPIO |
| Encoder A | ENC_A_A / ENC_A_B | PB4 / PB5 | TIM3 encoder |
| Encoder B | ENC_B_A / ENC_B_B | PB6 / PB7 | TIM4 encoder |

## D-Frame Polarity Convention

The Yahboom 8-LP sensor D-frame uses inverted logic: **0 = black line detected = LED on**. When using the digital cache (`s_digital_cache`), `lane_on(index)` returns `true` when the value is `0U`.

## TODO

- PCB project file (EasyEDA Pro): `docs_reference/ProPrj_TDPS PCB_2026-06-03.epro2` (version 2026-06-03).
- Final wiring table and PCB version correspondence.
- Battery, motor, wheel diameter, encoder CPR, actual power tree.
- Screen project source files and current serial parameters.
- Sensor mounting scheme for left/right fork avoidance.
