#ifndef LF_USE_STM32F4_HAL_PORT

#include "lf_platform.h"

#include <stddef.h>

/*
 * Stub 平台实现：
 * 1. 方便在没有 MCU HAL 的环境下做静态编译/语法检查。
 * 2. 实际上板时请启用 LF_USE_STM32F4_HAL_PORT 并使用 HAL 端口文件。
 */

static uint32_t s_fake_ms = 0U;

void LF_Platform_BoardInit(void)
{
    s_fake_ms = 0U;
}

uint32_t LF_Platform_GetMillis(void)
{
    return s_fake_ms;
}

void LF_Platform_DelayMs(uint32_t ms)
{
    s_fake_ms += ms;
}

void LF_Platform_ReadLineSensorRaw(uint16_t out_raw[LF_SENSOR_COUNT])
{
    uint32_t i;

    if (out_raw == NULL) {
        return;
    }

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        bool active_high = g_lf_config.sensor_digital_active_high;

        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            out_raw[i] = active_high ? 0U : 4095U;
        }

        if (LF_SENSOR_COUNT >= 8U) {
            /* 默认模拟中间两路“在线”。 */
            out_raw[3] = active_high ? 4095U : 0U;
            out_raw[4] = active_high ? 4095U : 0U;
        }
        return;
    }

    /*
     * 默认模拟“中间在线”场景。
     * 真实硬件端口应返回 ADC 原始值。
     */
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_raw[i] = 1500U;
    }

    if (LF_SENSOR_COUNT >= 8U) {
        out_raw[2] = 2500U;
        out_raw[3] = 3300U;
        out_raw[4] = 3300U;
        out_raw[5] = 2500U;
    }
}

uint16_t LF_Platform_RadarRead(uint8_t *out_buf, uint16_t max_len)
{
    (void)out_buf;
    (void)max_len;
    return 0U;
}

void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    (void)left_cmd;
    (void)right_cmd;
}

void LF_Platform_SetStatusLed(bool on)
{
    (void)on;
}

bool LF_Platform_IsStartButtonPressed(void)
{
    return false;
}

void LF_Platform_DebugPrint(const char *msg)
{
    (void)msg;
}

#endif /* LF_USE_STM32F4_HAL_PORT */
