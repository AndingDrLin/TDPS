# 无线通信设计文档（Task 2）

## 1. 任务目标

小车在巡线过程中经过拱门（Arch）时，通过 LoRa 无线模块向拱门接收端发送报文，报文包含：
- 队伍编号与队名
- 检查点 ID
- 从比赛开始到当前的时间戳

对应课程任务：**Task 2 — Wireless Communication**。

## 2. 硬件方案

### 2.1 无线模块

- **型号**：EWM22A-900BWL22S
- **频段**：850–930 MHz（LoRa 调制）
- **接口**：UART（8N1，默认 115200 bps）
- **配置方式**：AT 指令（模式 0 下通过 UART 发送）
- **数据传输**：模式 1（UART/LoRa 透传），写入 UART 的数据直接由模块进行 LoRa 发射
- **AUX 引脚**：高电平表示模块空闲/就绪，低电平表示忙碌

### 2.2 MCU 连接

| 信号 | MCU 引脚 | 说明 |
|------|----------|------|
| 模块 TXD → MCU RXD | PA3（USART2_RX） | 模块发送 → MCU 接收 |
| MCU TXD → 模块 RXD | PA2（USART2_TX） | MCU 发送 → 模块接收 |
| AUX | PB0 | 模块状态指示 |
| 调试 TX | PA9（USART1_TX） | 调试串口输出 |
| 调试 RX | PA10（USART1_RX） | 调试串口输入 |

## 3. 软件架构

采用分层设计，自底向上：

```
┌─────────────────────────────────────┐
│          应用层 (wl_app)            │
│  状态机 · 比赛计时 · 检查点触发      │
├──────────┬──────────────────────────┤
│ 协议层    │      驱动层              │
│ wl_protocol │   wl_lora             │
│ 报文打包   │   AT指令 · 数据发射     │
├──────────┴──────────────────────────┤
│      平台抽象层 (wl_platform)        │
│  UART · GPIO · 定时 统一接口         │
├─────────────────────────────────────┤
│  STM32F1 HAL 实现 │ PC 桩实现        │
└─────────────────────────────────────┘
```

### 3.1 平台抽象层（PAL）

- `wl_platform.h` 定义统一接口
- `wl_platform_stm32f1.c` 提供真实硬件实现（中断驱动 UART 接收 + 环形缓冲区）
- `wl_platform_stub.c` 提供 PC 端桩实现（printf 模拟 UART，自动回复 AT_OK）

### 3.2 LoRa 驱动层

- 封装 AT 指令的发送与响应解析
- 初始化流程：上电等待 → 进入配置模式 → 逐项设置参数 → 切换到透传模式
- 数据发射：在透传模式下直接写入 UART

### 3.3 协议层

报文格式（纯 ASCII）：
```
TEAM=15,NAME=PTSD,CP=21,TIME=5000\n
```

### 3.4 应用层

状态机：
```
IDLE → READY → RUNNING → FINISHED
                  ↓
               ERROR
```

- **IDLE**：初始状态
- **READY**：LoRa 初始化完成，等待比赛开始
- **RUNNING**：比赛进行中，接受检查点通知
- **FINISHED**：比赛结束
- **ERROR**：初始化或通信出错

节流机制：两次发射间隔不小于 `WL_TX_MIN_INTERVAL_MS`（默认 500ms），防止信道拥堵。

## 4. 初始化流程

```
1. WL_Platform_Init()          — 初始化 UART、GPIO
2. 等待 AUX 高电平              — 模块上电自检完成
3. AT+HMODE=0                  — 进入配置模式
4. AT                          — 验证 AT 链路
5. AT+ADDR=0                   — 设置模块地址
6. AT+CHANNEL=18               — 设置信道（868.125 MHz）
7. AT+NETID=0                  — 设置网络 ID
8. AT+RATE=2                   — 设置空中速率（2.4 kbps）
9. AT+POWER=0                  — 设置发射功率（22 dBm）
10. AT+TRANS=0                 — 设置透明传输模式
11. AT+PACKET=0                — 设置分包长度（240 字节）
12. AT+HMODE=1                 — 切换到 UART/LoRa 透传模式
```

## 5. 与巡线模块的集成

巡线模块（`line_follow_v1`）在检测到拱门标记时，调用：

```c
#include "wl_app.h"

WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);  // 拱门 2.1
WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_2);  // 拱门 2.2
```

## 6. 测试方案

### 6.1 PC 端桩测试

```bash
cd code/wireless_v1/Src
gcc -I../Inc -o wl_test main.c wl_app.c wl_lora.c wl_protocol.c wl_config.c wl_platform_stub.c
./wl_test
```

验证项目：
- [x] AT 指令序列正确
- [x] 报文格式正确
- [x] 计时逻辑正确
- [x] 节流机制生效

### 6.2 硬件测试

1. 将代码烧录到 STM32F103C8T6
2. 连接 EWM22A 模块
3. 通过调试串口（USART1）观察日志输出
4. 使用另一个 EWM22A 模块（或拱门接收端）验证是否收到报文

## 7. 参考资料

- EWM22A-xxxBWL22S 用户手册（`slides/EWM22A-xxxBWL22S_UserManual_CN_V1.0.pdf`）
- 课程任务总览（`slides/L1b_Design-Tasks_An-Overview_2025-2026.pdf`）
