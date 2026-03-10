# TDPS Project

> 基于 STM32 的智能小车项目：巡线 + LoRa 通信 + 雷达避障

## Tasks

- **任务 1：巡线行驶**：沿黑线稳定通过弯道与路口，到达终点。
- **任务 2：无线通信**：在指定拱门区域通过 LoRa 发送队伍与计时信息。
- **任务 3：避障通行**：使用 24 GHz 及以上雷达判断障碍左右并选择空侧通过。

## Content

1. 总体设计与实现规划 [basic_design.md](docs/basic_design.md)。
2. 巡线子系统设计文档 [line_following.md](docs/line_following.md)
3. 巡线一期代码（可扩展框架）[firmware/line_follow_v1/README.md](firmware/line_follow_v1/README.md)

## Usage

本章节给出烧录编译程序的完整步骤，默认使用 STM32CubeIDE + HAL 工程。

### 1. 前置要求

1. 已安装 `STM32CubeIDE`（建议 1.14+）。
2. 已安装 `STM32CubeProgrammer`（可选，用于命令行烧录）。
3. 有一套可识别的 ST-LINK 下载器和驱动。
4. 已有或新建一个与主控匹配的 Cube 工程（如 `STM32F407VET6`）。

### 2. 添加源代码

1. 将 `code/line_follow_v1/Inc` 目录下所有头文件复制到你的工程 `Core/Inc`（或加入为额外 include 目录）。
2. 将 `code/line_follow_v1/Src` 目录下除 `main.c` 外的源码复制到你的工程 `Core/Src`。
3. 不要同时保留两份 `main.c`，推荐继续使用 Cube 生成的 `main.c`，在其中调用巡线框架。
4. 在工程编译宏中添加 `LF_USE_STM32F4_HAL_PORT`。
5. 确保 `lf_platform_stm32f4_hal.c` 被编译；`lf_platform_stub.c` 可保留（该宏启用后会自动失效）。

### 3. 设置外设参数

1. `ADC1 + DMA`：
   1. 打开扫描转换，通道数设置为传感器数量（当前默认 4 路）。
   2. DMA 设为 `Circular` 模式，持续更新传感器缓冲。
2. `TIM PWM`：
   1. 启用 2 个 PWM 通道（默认代码示例为 `TIM3 CH1/CH2`）。
   2. 推荐 PWM 频率 10~20 kHz，避免电机啸叫。
3. `GPIO`：
   1. 左右电机方向脚配置为推挽输出。
   2. 状态灯脚配置为输出。
   3. 启动按键脚配置为输入（根据接线选择上拉/下拉）。
4. `USART`（可选）：
   1. 用于调试输出（默认示例 `USART1`）。

### 4. 硬件映射

按你的实际接线修改：

`firmware/line_follow_v1/Inc/lf_port_stm32f4_hal.h`

至少确认以下宏：

1. ADC 句柄与 DMA 缓冲区。
2. 左右电机 PWM 定时器与通道。
3. 左右电机方向 GPIO。
4. LED 与按钮 GPIO。
5. 串口句柄（若启用调试串口）。

### 5. 调整 `main.c` 文件

在你的 Cube `main.c` 中加入：

```c
#include "lf_app.h"
#include "lf_platform.h"

volatile uint16_t g_lf_sensor_dma_buffer[LF_SENSOR_COUNT];

void LF_Port_SystemClock_Config(void) {
    SystemClock_Config();
}

void LF_Port_Peripheral_Init(void) {
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init(); // 可选
}

int main(void) {
    LF_Platform_BoardInit();
    LF_App_Init();
    while (1) {
        LF_App_RunStep();
    }
}
```

说明：`LF_Port_SystemClock_Config` 和 `LF_Port_Peripheral_Init` 是弱符号覆盖点，用来接管你现有 Cube 初始化流程。

### 6. 编译

1. 在 CubeIDE 中执行 `Project -> Clean`。
2. 执行 `Project -> Build Project`。
3. 确认无编译报错后，检查生成的 `.elf` 文件。

### 7. 烧录

方式 A（CubeIDE）：

1. ST-LINK 连接开发板和电脑。
2. `Run -> Run Configurations... -> STM32 Cortex-M C/C++ Application`。
3. 选择工程后点击 `Run`（或 `Debug`）。

方式 B（命令行，可选）：

```bash
STM32_Programmer_CLI -c port=SWD -w build/<your_project>.elf -v -rst
```

### 8. 检查清单

1. 上电后程序会进入自动标定（原地小幅左右转向）。
2. 标定结束后进入巡线运行状态。
3. 若方向反了，仅修改 `lf_port_stm32f4_hal.h` 中的前进方向电平宏。
4. 若能跑但抖动大，优先调 `firmware/line_follow_v1/Src/lf_config.c` 的：
   1. `base_speed`
   2. `kp`
   3. `kd`
   4. `line_detect_min_sum`

### 9. 常见问题

1. 电机不转：
   1. 检查 PWM 定时器是否启动。
   2. 检查电机驱动使能脚与方向脚电平是否正确。
2. 传感器值不变化：
   1. 检查 ADC 通道顺序和 DMA 缓冲映射。
   2. 检查传感器供电与地线共地。
3. 频繁丢线：
   1. 延长标定时间 `calibration_duration_ms`。
   2. 降低 `base_speed`，增大 `line_detect_min_sum` 的鲁棒区间。
4. 小车左右摆振明显：
   1. 先减小 `kp`。
   2. 再适当增大 `kd`。
   3. 保持 `ki = 0`，稳定后再小幅引入积分。

## Reference

1. [SayanSeth/Line-Follower-Robot](https://github.com/SayanSeth/Line-Follower-Robot)
2. [sametoguten/STM32-Line-Follower-with-PID](https://github.com/sametoguten/STM32-Line-Follower-with-PID)
3. [navidadelpour/line-follower-robot](https://github.com/navidadelpour/line-follower-robot)
4. [FredBill1/RT1064_Smartcar](https://github.com/FredBill1/RT1064_Smartcar)
5. [ittuann/Enterprise_E](https://github.com/ittuann/Enterprise_E)
6. [Awesome-IntelligentCarRace](https://ittuann.github.io/Awesome-IntelligentCarRace/)
