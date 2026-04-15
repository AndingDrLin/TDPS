# line_follow_v1

`line_follow_v1` 是巡线主链路的最小可用实现，目标是先把“稳定可跑 + 可回归”做扎实，再在同一框架上扩展 LoRa 和雷达逻辑。

## 功能边界

当前实现包含：

- 传感器标定（启动后自动进入标定窗口）
- 8 路灰度输入（标定、归一化、低通滤波、加权位置估计）
- 基于位置偏差的 PID 巡线
- 丢线恢复与恢复超时停车
- 雷达串口帧解析（非阻塞）与避障状态输出（CLEAR/WARN/BLOCK）
- 统一平台抽象层（PAL）
- 离线仿真回归（测试代码位于 `TDPS-Simulator/sim_tests`）

当前实现不包含：

- LD2410S 完整官方协议全字段解析（当前为保守默认帧）
- 实机雷达串口端口映射（HAL 端口中保留 TODO）

LoRa 通信由 `wireless_v1` 与 `wireless_hooks` 侧接入，主控制环保持非阻塞。

## 目录

```text
code/line_follow_v1/
├── Inc/
│   ├── lf_app.h
│   ├── lf_chassis.h
│   ├── lf_config.h
│   ├── lf_control.h
│   ├── lf_future_hooks.h
│   ├── lf_platform.h
│   ├── lf_port_stm32f4_hal.h
│   ├── lf_radar.h
│   └── lf_sensor.h
└── Src/
    ├── lf_app.c
    ├── lf_chassis.c
    ├── lf_config.c
    ├── lf_control.c
    ├── lf_future_hooks.c
    ├── lf_platform_stm32f4_hal.c
    ├── lf_platform_stub.c
    ├── lf_radar.c
    ├── lf_sensor.c
    ├── wireless_hooks.c
    └── main.c
```

## 运行时状态机

`LF_AppState` 定义于 `Inc/lf_app.h`：

- `WAIT_START`：等待按键或自动启动延时
- `CALIBRATING`：左右摆动采样，更新 min/max
- `RUNNING`：正常巡线
- `RECOVERING`：丢线后按最后方向搜索
- `STOPPED`：恢复超时后停车
- `FAULT`：异常保护态

## 调参入口

所有关键参数集中在 `Src/lf_config.c` 的 `g_lf_config`：

- 速度与控制：`base_speed`、`kp`、`ki`、`kd`
- 判线与滤波：`line_detect_min_sum`、`sensor_filter_alpha`
- 恢复策略：`recover_turn_speed`、`recover_timeout_ms`
- 雷达阈值：`radar_trigger_distance_mm`、`radar_release_distance_mm`
- 雷达去抖：`radar_debounce_frames`、`radar_frame_timeout_ms`
- 避障减速：`obstacle_warn_speed`

建议调参顺序：

1. `base_speed`
2. `kp`
3. `kd`
4. `line_detect_min_sum`
5. 恢复参数

## STM32CubeIDE 接入步骤

### 1. 拷贝源码

- 把 `Inc/` 头文件加入工程 include 路径。
- 把 `Src/` 源文件加入工程（通常不使用本目录 `main.c`）。

### 2. 打开 HAL 端口宏

在编译宏中添加：

```c
LF_USE_STM32F4_HAL_PORT
```

### 3. 修改板级映射

按接线修改：`Inc/lf_port_stm32f4_hal.h`

至少确认：

- ADC 句柄
- 左右 PWM 定时器与通道
- 左右方向 GPIO
- LED / 启动按钮 GPIO
- 调试串口（可选）

### 4. 提供板级符号

在你的工程中提供：

```c
volatile uint16_t g_lf_sensor_dma_buffer[LF_SENSOR_COUNT];
void LF_Port_SystemClock_Config(void);
void LF_Port_Peripheral_Init(void);
```

说明：`LF_Port_SystemClock_Config` 和 `LF_Port_Peripheral_Init` 在 HAL 端口中为弱符号，可由用户工程覆盖。

### 5. 外设要求

- ADC：DMA 连续采样，缓冲长度 `>= LF_SENSOR_COUNT`
- PWM：左右电机各 1 路
- GPIO：方向脚、LED、按钮
- 雷达 UART：建议中断 + 环形缓冲，平台层通过 `LF_Platform_RadarRead()` 非阻塞读取

### 6. 编译与上板

- 在 CubeIDE 中 `Clean` -> `Build`
- 上板后观察：启动 -> 标定 -> RUNNING

说明：`LF_SENSOR_COUNT` 默认已升级为 `8`，若需与旧硬件兼容可在分支中临时回退为 `4`。

## 扩展点（LoRa / 雷达）

接口定义：`Inc/lf_future_hooks.h`

- `LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id)`
- `LF_Hook_OnReservedObstacleWindow(void)`

默认实现为弱符号空函数，后续在其他源文件中提供同名非弱符号函数即可覆盖。

## 相关文档

- 仓库入口：`README.md`
- 仿真测试：`TDPS-Simulator/sim_tests/line_follow_v1/README.md`
- 回归流程：`docs/testing/quickstart_3min.md`
