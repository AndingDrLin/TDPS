#ifdef LF_USE_STM32F4_HAL_PORT

#include "lf_platform.h"

#include <stddef.h>
#include <string.h>

#include "lf_debug_monitor.h"
#include "lf_port_stm32f4_hal.h"
#include "lf_radar_uart.h"

/*
 * 说明：
 * 1. 该文件是可直接上板的 HAL 端口实现。
 * 2. 时钟与外设初始化函数由用户工程提供（CubeMX 生成或手写）。
 * 3. 仅要修改 lf_port_stm32f4_hal.h 的宏即可适配不同接线。
 */

/* 用户可在其他文件中覆盖这两个弱函数。 */
__weak void LF_Port_SystemClock_Config(void) {}
__weak void LF_Port_Peripheral_Init(void) {}

/*
 * 用户可覆盖该弱函数以读取 8-LP 的 X1~X8 GPIO 电平。
 * 返回 true 表示 out_level 有效，false 表示未实现。
 */
__weak bool LF_Port_ReadLineSensorDigital(uint8_t out_level[LF_SENSOR_COUNT])
{
    (void)out_level;
    return false;
}

static volatile bool s_lf_debug_tx_busy = false;
static char s_lf_debug_tx_buffer[384];

static GPIO_PinState invert_pin_state(GPIO_PinState s)
{
    return (s == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

static uint32_t cmd_to_pwm(int16_t cmd, TIM_HandleTypeDef *htim)
{
    uint32_t arr;
    int32_t abs_cmd = cmd;

    if (abs_cmd < 0) {
        abs_cmd = -abs_cmd;
    }
    if (abs_cmd > 1000) {
        abs_cmd = 1000;
    }

    arr = __HAL_TIM_GET_AUTORELOAD(htim);
    return (uint32_t)((abs_cmd * (int32_t)arr) / 1000);
}

static void set_single_motor(int16_t cmd,
                             GPIO_TypeDef *dir_port,
                             uint16_t dir_pin,
                             GPIO_PinState forward_level,
                             TIM_HandleTypeDef *pwm_timer,
                             uint32_t pwm_channel)
{
    GPIO_PinState level = (cmd >= 0) ? forward_level : invert_pin_state(forward_level);
    uint32_t pwm = cmd_to_pwm(cmd, pwm_timer);

    HAL_GPIO_WritePin(dir_port, dir_pin, level);
    __HAL_TIM_SET_COMPARE(pwm_timer, pwm_channel, pwm);
}

void LF_Platform_BoardInit(void)
{
    HAL_Init();
    LF_Port_SystemClock_Config();
    LF_Port_Peripheral_Init();

    HAL_TIM_PWM_Start(&LF_PORT_LEFT_PWM_TIMER_HANDLE, LF_PORT_LEFT_PWM_CHANNEL);
    HAL_TIM_PWM_Start(&LF_PORT_RIGHT_PWM_TIMER_HANDLE, LF_PORT_RIGHT_PWM_CHANNEL);

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_ANALOG_ADC) {
        HAL_ADC_Start_DMA(&LF_PORT_ADC_HANDLE, g_lf_sensor_dma_buffer, LF_SENSOR_COUNT);
    }

    if (g_lf_config.radar_enable) {
        LF_RadarUart_Init();
    }
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
    size_t i;

    if (out_raw == NULL) {
        return;
    }

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        uint8_t level[LF_SENSOR_COUNT];

        if (LF_Port_ReadLineSensorDigital(level)) {
            for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
                out_raw[i] = (level[i] != 0U) ? 4095U : 0U;
            }
            return;
        }
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        out_raw[i] = (uint16_t)(g_lf_sensor_dma_buffer[i] & 0x0FFFU);
    }
}

uint16_t LF_Platform_RadarRead(uint8_t *out_buf, uint16_t max_len)
{
    return LF_RadarUart_Read(out_buf, max_len);
}

void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    LF_DebugMonitor_OnMotorCommand(left_cmd, right_cmd);

    if (LF_DebugMonitor_IsNoCarMode()) {
        return;
    }

    set_single_motor(left_cmd,
                     LF_PORT_LEFT_DIR_GPIO_PORT,
                     LF_PORT_LEFT_DIR_PIN,
                     LF_PORT_LEFT_FORWARD_LEVEL,
                     &LF_PORT_LEFT_PWM_TIMER_HANDLE,
                     LF_PORT_LEFT_PWM_CHANNEL);

    set_single_motor(right_cmd,
                     LF_PORT_RIGHT_DIR_GPIO_PORT,
                     LF_PORT_RIGHT_DIR_PIN,
                     LF_PORT_RIGHT_FORWARD_LEVEL,
                     &LF_PORT_RIGHT_PWM_TIMER_HANDLE,
                     LF_PORT_RIGHT_PWM_CHANNEL);
}

void LF_Platform_SetStatusLed(bool on)
{
    GPIO_PinState level = on ? LF_PORT_LED_ON_LEVEL : invert_pin_state(LF_PORT_LED_ON_LEVEL);
    HAL_GPIO_WritePin(LF_PORT_LED_GPIO_PORT, LF_PORT_LED_PIN, level);
}

bool LF_Platform_IsStartButtonPressed(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(LF_PORT_BTN_GPIO_PORT, LF_PORT_BTN_PIN);
    return (s == LF_PORT_BTN_PRESSED_LEVEL);
}

void LF_Platform_DebugPrint(const char *msg)
{
#if LF_PORT_ENABLE_DEBUG_UART
    size_t len;

    if (msg == NULL || s_lf_debug_tx_busy) {
        return;
    }

    len = strlen(msg);
    if (len >= sizeof(s_lf_debug_tx_buffer)) {
        len = sizeof(s_lf_debug_tx_buffer) - 1U;
    }

    memcpy(s_lf_debug_tx_buffer, msg, len);
    s_lf_debug_tx_buffer[len] = '\0';

    s_lf_debug_tx_busy = true;
    if (HAL_UART_Transmit_IT(&LF_PORT_DEBUG_UART_HANDLE, (uint8_t *)s_lf_debug_tx_buffer, (uint16_t)len) != HAL_OK) {
        s_lf_debug_tx_busy = false;
    }
#else
    (void)msg;
#endif
}

void LF_Port_UartRxCpltCallback(UART_HandleTypeDef *huart)
{
    LF_RadarUart_RxCpltCallback(huart);
}

void LF_Port_UartErrorCallback(UART_HandleTypeDef *huart)
{
    LF_RadarUart_ErrorCallback(huart);
}

void LF_Port_UartTxCpltCallback(UART_HandleTypeDef *huart)
{
#if LF_PORT_ENABLE_DEBUG_UART
    if (huart != NULL && huart->Instance == LF_PORT_DEBUG_UART_HANDLE.Instance) {
        s_lf_debug_tx_busy = false;
    }
#else
    (void)huart;
#endif
}

#endif /* LF_USE_STM32F4_HAL_PORT */
