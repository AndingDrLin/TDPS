#include "main.h"

#include <stddef.h>
#include <string.h>

#include "lf_platform.h"
#include "lf_sensor_uart.h"

void LF_Platform_BoardInit(void)
{
}

uint32_t LF_Platform_GetMillis(void)
{
    return HAL_GetTick();
}

void LF_Platform_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void LF_Platform_ReadLineSensorRaw(uint16_t out_raw[LF_SENSOR_COUNT])
{
    uint16_t analog[LF_SENSOR_COUNT];
    uint32_t i;

    if (out_raw == NULL) {
        return;
    }

    if (LF_SensorUart_GetAnalogFrame(analog)) {
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            out_raw[i] = (analog[i] > 4095U) ? 4095U : analog[i];
        }
        return;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_raw[i] = 0U;
    }
}

uint16_t LF_Platform_RadarRead(uint8_t *out_buf, uint16_t max_len)
{
    (void)out_buf;
    (void)max_len;
    return 0U;
}

#ifndef LF_USE_STM32F4_HAL_PORT
void LF_PlatformStub_RadarInject(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

void LF_PlatformStub_RadarClear(void)
{
}

void LF_PlatformStub_SetLineSensorRaw(const uint16_t raw[LF_SENSOR_COUNT])
{
    (void)raw;
}

void LF_PlatformStub_ClearLineSensorRaw(void)
{
}

const char *LF_PlatformStub_GetLastDebugLine(void)
{
    return "";
}
#endif

void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    g_line_motor_left_applied = left_cmd;
    g_line_motor_right_applied = right_cmd;
}

void LF_Platform_SetStatusLed(bool on)
{
    GPIO_PinState level = on ? LINE_STATUS_LED_ON_LEVEL :
                          ((LINE_STATUS_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LINE_STATUS_LED_GPIO_Port, LINE_STATUS_LED_Pin, level);
}

bool LF_Platform_IsStartButtonPressed(void)
{
    return true;
}

void LF_Platform_DebugPrint(const char *msg)
{
    (void)msg;
}
