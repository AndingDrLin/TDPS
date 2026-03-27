# Wireless Communication v1 — 无线通信模块

## 概述

本模块实现 **Task 2（无线通信）** 的全部功能：通过 EWM22A-900BWL22S LoRa 模块，在小车经过拱门检查点时向拱门接收端发送包含队伍编号、队名和时间戳的报文。

## 硬件清单

| 元件 | 型号 / 规格 |
|------|------------|
| MCU | STM32F103C8T6 |
| 无线模块 | EWM22A-900BWL22S（LoRa 850–930 MHz） |
| 通信接口 | UART（默认 115200 bps，8N1） |
| AUX 引脚 | PB0（输入，高电平 = 模块就绪） |
| 模块 UART | USART2（PA2=TX, PA3=RX） |
| 调试 UART | USART1（PA9=TX, PA10=RX） |

## 软件架构

```
┌─────────────────────────────────────┐
│          wl_app（应用层）            │  状态机、比赛计时、检查点触发
├──────────┬──────────────────────────┤
│ wl_protocol │      wl_lora          │  报文打包 │ AT指令驱动
├──────────┴──────────────────────────┤
│       wl_platform（平台抽象层）       │  UART/GPIO/定时 统一接口
├─────────────────────────────────────┤
│  wl_platform_stm32f1 │ wl_platform_stub │  真实HAL │ PC桩实现
└─────────────────────────────────────┘
```

## 文件说明

### Inc/（头文件）

| 文件 | 说明 |
|------|------|
| `wl_config.h` | 全局配置参数（LoRa 地址/信道/功率、UART 波特率、超时等） |
| `wl_platform.h` | 平台抽象层接口定义（UART/GPIO/定时） |
| `wl_port_stm32f1.h` | STM32F103 引脚映射与外设配置 |
| `wl_lora.h` | LoRa 驱动层接口（AT 指令、数据发射） |
| `wl_protocol.h` | 报文协议格式定义与打包 API |
| `wl_app.h` | 应用层状态机接口 |

### Src/（源文件）

| 文件 | 说明 |
|------|------|
| `wl_config.c` | 默认运行时配置实例 |
| `wl_platform_stm32f1.c` | STM32F103 HAL 实现（需定义 `WL_USE_STM32F1_HAL_PORT`） |
| `wl_platform_stub.c` | PC 桩实现（离线测试用） |
| `wl_lora.c` | LoRa 驱动实现（AT 指令交互与数据发射） |
| `wl_protocol.c` | 报文打包实现 |
| `wl_app.c` | 应用层状态机实现（计时、检查点、节流） |
| `main.c` | 独立测试入口（同时包含 PC 端与 STM32 端） |

## 报文格式

```
TEAM=15,NAME=PTSD,CP=21,TIME=5000\n
```

字段说明：
- `TEAM` — 队伍编号
- `NAME` — 队伍名称
- `CP` — 检查点 ID（21 = 拱门 2.1，22 = 拱门 2.2）
- `TIME` — 从比赛开始经过的毫秒数

## PC 端测试

```bash
cd code/wireless_v1/Src
gcc -I../Inc -o wl_test main.c wl_app.c wl_lora.c wl_protocol.c wl_config.c wl_platform_stub.c
./wl_test
```

桩模式下：
- 所有 UART 发送内容打印到终端
- AT 指令自动返回 `AT_OK` 模拟成功
- 时间通过 `WL_Platform_DelayMs()` 手动推进

## STM32 编译

在 Keil / STM32CubeIDE 工程中：
1. 添加 `Inc/` 到头文件搜索路径
2. 添加 `Src/` 下所有 `.c` 文件（排除 `wl_platform_stub.c`）
3. 在编译器全局宏中定义 `WL_USE_STM32F1_HAL_PORT`
4. 确保工程包含 STM32F1xx HAL 库

## 巡线模块集成

巡线模块在检测到拱门时，调用以下 API：

```c
#include "wl_app.h"

/* 初始化时调用一次 */
WL_App_Init();
WL_App_StartRace();

/* 经过拱门时调用 */
WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);  /* 拱门 2.1 */
WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_2);  /* 拱门 2.2 */
```

## 配置修改

所有参数集中在 `wl_config.h` 中，包括：
- LoRa 地址、信道、网络 ID、空中速率、发射功率
- UART 波特率
- 超时与延时参数
- 检查点 ID 映射
