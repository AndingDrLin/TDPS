# Line Follow V1 (STM32)

这个版本是“巡线一期最小可用实现”：

1. 支持光照标定。
2. 支持基础 PID 巡线（直线 + 简单曲线）。
3. 支持丢线恢复。
4. 预留 LoRa/雷达扩展钩子，不影响当前主流程。

## 1. 目录说明

```text
Inc/
  lf_config.h                // 参数定义（只声明）
  lf_platform.h              // 平台抽象接口
  lf_port_stm32f4_hal.h      // STM32F4 HAL 端口映射（你们需要修改）
  lf_sensor.h                // 传感器与标定
  lf_control.h               // PID 控制
  lf_chassis.h               // 底盘输出
  lf_future_hooks.h          // 后续任务预留钩子
  lf_app.h                   // 应用状态机
Src/
  lf_config.c                // 参数集中配置（你们主要调这个）
  lf_sensor.c
  lf_control.c
  lf_chassis.c
  lf_app.c
  lf_future_hooks.c
  lf_platform_stm32f4_hal.c  // 实机端口
  lf_platform_stub.c         // 非 HAL 环境下的编译兜底
  main.c
```

## 2. 参数集中位置

所有调参集中在：

`Src/lf_config.c`

建议调参顺序：

1. `base_speed`
2. `kp`
3. `kd`
4. `line_detect_min_sum`
5. `recover_turn_speed` / `recover_timeout_ms`

## 3. 直接烧录接入步骤（CubeIDE / HAL）

1. 将 `Inc/*` 和 `Src/*` 加入你的 STM32Cube 工程。
2. 在工程编译宏中添加：`LF_USE_STM32F4_HAL_PORT`
3. 在 `Inc/lf_port_stm32f4_hal.h` 中按实际接线修改：
   1. PWM 定时器和通道
   2. 左右电机方向脚
   3. LED / 启动按钮
   4. 串口句柄（可选）
4. 在你自己的板级文件中提供：
   1. `volatile uint16_t g_lf_sensor_dma_buffer[LF_SENSOR_COUNT];`
   2. `LF_Port_SystemClock_Config(void)`
   3. `LF_Port_Peripheral_Init(void)`（可直接调用 CubeMX 生成的 `MX_*_Init`）
5. 确保 ADC 已配置为 DMA 连续采样，前 `LF_SENSOR_COUNT` 个槽位对应巡线传感器。
6. 上电后程序会自动进入标定，再进入巡线运行。

## 4. 软件工程约束

1. ISR 不做复杂控制，只做外设搬运和快速标志更新。
2. 状态机统一由 `LF_App_RunStep` 驱动，避免模块抢占写电机。
3. 后续功能必须优先走 `lf_future_hooks`，避免污染巡线主链路。
4. 每次调参只改 `lf_config.c`，并记录“参数 -> 场景 -> 结果”。

## 5. 后续扩展入口

在 `lf_future_hooks.c` 中提供了弱符号空实现，可覆盖：

1. `LF_Hook_OnReservedCheckpoint(...)`：后续接 LoRa 发送窗口触发。
2. `LF_Hook_OnReservedObstacleWindow()`：后续接雷达决策窗口触发。
