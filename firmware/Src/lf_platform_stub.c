#ifndef LF_USE_STM32F4_HAL_PORT

#include "lf_platform.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lf_debug_monitor.h"

/*
 * Stub 平台实现：
 * 1. 方便在没有 MCU HAL 的环境下做静态编译/语法检查。
 * 2. 实际上板时请启用 LF_USE_STM32F4_HAL_PORT 并使用 HAL 端口文件。
 */

#define LF_STUB_RADAR_BUFFER_SIZE (256U)

static uint32_t s_fake_ms = 0U;
static uint8_t s_radar_buf[LF_STUB_RADAR_BUFFER_SIZE];
static uint16_t s_radar_head = 0U;
static uint16_t s_radar_tail = 0U;
static bool s_line_override_enabled = false;
static uint16_t s_line_override[LF_SENSOR_COUNT];
static char s_debug_last_line[512];

void LF_Platform_BoardInit(void)
{
    s_fake_ms = 0U;
    s_radar_head = 0U;
    s_radar_tail = 0U;
    s_line_override_enabled = false;
    s_debug_last_line[0] = '\0';
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

    if (s_line_override_enabled) {
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            out_raw[i] = s_line_override[i];
        }
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

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_raw[i] = ((s_fake_ms / 100U) & 1U) ? 900U : 1500U;
    }

    if (LF_SENSOR_COUNT >= 8U) {
        out_raw[2] = ((s_fake_ms / 100U) & 1U) ? 2500U : 900U;
        out_raw[3] = ((s_fake_ms / 100U) & 1U) ? 3300U : 900U;
        out_raw[4] = ((s_fake_ms / 100U) & 1U) ? 3300U : 900U;
        out_raw[5] = ((s_fake_ms / 100U) & 1U) ? 2500U : 900U;
    }
}

uint16_t LF_Platform_RadarRead(uint8_t *out_buf, uint16_t max_len)
{
    uint16_t count = 0U;

    if (out_buf == NULL) {
        return 0U;
    }

    while (count < max_len && s_radar_tail != s_radar_head) {
        out_buf[count++] = s_radar_buf[s_radar_tail];
        s_radar_tail = (uint16_t)((s_radar_tail + 1U) % LF_STUB_RADAR_BUFFER_SIZE);
    }

    return count;
}

void LF_PlatformStub_RadarInject(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL) {
        return;
    }

    for (i = 0U; i < len; ++i) {
        uint16_t next = (uint16_t)((s_radar_head + 1U) % LF_STUB_RADAR_BUFFER_SIZE);
        if (next == s_radar_tail) {
            break;
        }
        s_radar_buf[s_radar_head] = data[i];
        s_radar_head = next;
    }
}

void LF_PlatformStub_RadarClear(void)
{
    s_radar_head = 0U;
    s_radar_tail = 0U;
}

void LF_PlatformStub_SetLineSensorRaw(const uint16_t raw[LF_SENSOR_COUNT])
{
    uint32_t i;

    if (raw == NULL) {
        return;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_line_override[i] = raw[i];
    }
    s_line_override_enabled = true;
}

void LF_PlatformStub_ClearLineSensorRaw(void)
{
    s_line_override_enabled = false;
}

void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    LF_DebugMonitor_OnMotorCommand(left_cmd, right_cmd);
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
    if (msg != NULL) {
        snprintf(s_debug_last_line, sizeof(s_debug_last_line), "%s", msg);
        fputs(msg, stdout);
        fflush(stdout);
    }
}

const char *LF_PlatformStub_GetLastDebugLine(void)
{
    return s_debug_last_line;
}

#endif /* LF_USE_STM32F4_HAL_PORT */
