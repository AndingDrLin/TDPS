#ifndef LF_PLATFORM_H
#define LF_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

/*
 * 平台抽象层（PAL）
 * 目标：
 * 1. 让业务代码不依赖具体 MCU 外设细节。
 * 2. 后续切换硬件平台时，仅需替换本层实现。
 */

/* 板级初始化：时钟、GPIO、PWM、ADC、DMA、串口等。 */
void LF_Platform_BoardInit(void);

/* 读取单调递增毫秒计数。 */
uint32_t LF_Platform_GetMillis(void);

/* 毫秒级阻塞延时（仅用于初始化阶段，不建议在控制循环中调用）。 */
void LF_Platform_DelayMs(uint32_t ms);

/* 读取巡线传感器原始值。范围由 ADC 分辨率决定（如 0~4095）。 */
void LF_Platform_ReadLineSensorRaw(uint16_t out_raw[LF_SENSOR_COUNT]);

/* 写入电机命令（-1000~1000）。正负号表示方向，绝对值表示速度。 */
void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd);

/* 状态灯控制。 */
void LF_Platform_SetStatusLed(bool on);

/* 启动键读取（按下返回 true）。 */
bool LF_Platform_IsStartButtonPressed(void);

/* 轻量调试输出。没有串口时可空实现。 */
void LF_Platform_DebugPrint(const char *msg);

#endif /* LF_PLATFORM_H */
