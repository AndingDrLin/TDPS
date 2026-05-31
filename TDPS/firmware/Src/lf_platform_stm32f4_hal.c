#ifdef LF_USE_STM32F4_HAL_PORT

#include "lf_platform.h"

#include <stddef.h>
#include <string.h>

#include "lf_debug_monitor.h"
#include "lf_port_stm32f4_hal.h"
#include "lf_radar_uart.h"
#include "lf_sensor_uart.h"
#include "lf_watch_debug.h"

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
#if LF_PORT_ENABLE_DEBUG_UART
static char s_lf_debug_tx_buffer[384];
#endif
static volatile uint8_t s_lf_sensor_uart_rx_byte;
static uint32_t s_last_sensor_command_ms;

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

/*
 * 启动 DWT 周期计数器（Cortex-M4 内建，无需占用额外定时器）。
 * 168MHz 主频下每 6ns 递增一次，用于微秒级 dt_s 计算。
 * 32 位计数器约 25.5 秒回绕，差值运算对无符号回绕安全。
 */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void LF_Platform_BoardInit(void)
{
    HAL_Init();
    LF_Port_SystemClock_Config();
    LF_Port_Peripheral_Init();

    HAL_TIM_PWM_Start(&LF_PORT_LEFT_PWM_TIMER_HANDLE, LF_PORT_LEFT_PWM_CHANNEL);
    HAL_TIM_PWM_Start(&LF_PORT_RIGHT_PWM_TIMER_HANDLE, LF_PORT_RIGHT_PWM_CHANNEL);

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_ANALOG_ADC) {
        HAL_ADC_Start_DMA(&LF_PORT_ADC_HANDLE, (uint32_t *)g_lf_sensor_dma_buffer, LF_SENSOR_COUNT);
    }

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL) {
        LF_SensorUart_Init(&LF_PORT_SENSOR_UART_HANDLE);
        /*
         * 中断优先级阶梯（从高到低）：
         * (未来)TIM 控制中断 = 3 → 传感器 UART RX = 4 → 雷达 UART RX = 5 → UART TX Complete = 6。
         * 控制周期是刚性需求，传感器数据丢一个字节则整帧作废，雷达可容忍偶尔丢帧。
         */
        HAL_NVIC_SetPriority(LF_PORT_SENSOR_UART_IRQN,
                             LF_PORT_SENSOR_UART_IRQ_PRIORITY,
                             LF_PORT_SENSOR_UART_IRQ_SUB_PRIORITY);
        HAL_NVIC_EnableIRQ(LF_PORT_SENSOR_UART_IRQN);
        (void)HAL_UART_Receive_IT(&LF_PORT_SENSOR_UART_HANDLE,
                                   (uint8_t *)&s_lf_sensor_uart_rx_byte, 1U);
        HAL_Delay(1000U);
        (void)LF_SensorUart_SendCommand(LF_SENSOR_UART_COMMAND_BOTH);
        s_last_sensor_command_ms = HAL_GetTick();
    }

    if (g_lf_config.radar_enable) {
        LF_RadarUart_Init();
        LF_RadarUart_SwitchToStandardMode();
    }

    dwt_init();
}

uint32_t LF_Platform_GetMillis(void)
{
    return HAL_GetTick();
}

/* DWT 微秒时间戳：将 CPU 周期数换算为微秒，用于高精度 dt_s。 */
uint32_t LF_Platform_GetMicros(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
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

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL) {
        uint16_t values[LF_SENSOR_COUNT];

        if (LF_SensorUart_GetAnalogFrame(values)) {
            for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
                out_raw[i] = (values[i] > 4095U) ? 4095U : values[i];
            }
            return;
        }
        if ((uint32_t)(HAL_GetTick() - s_last_sensor_command_ms) >= 500U) {
            (void)LF_SensorUart_SendCommand(LF_SENSOR_UART_COMMAND_BOTH);
            s_last_sensor_command_ms = HAL_GetTick();
        }
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            out_raw[i] = 0U;
        }
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
    bool no_car_mode;

    LF_DebugMonitor_OnMotorCommand(left_cmd, right_cmd);
    no_car_mode = LF_DebugMonitor_IsNoCarMode();
    LF_WatchDebug_UpdateMotor(left_cmd, right_cmd, no_car_mode ? 1U : 0U);

    if (no_car_mode) {
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
    LF_WatchDebug_UpdateMotor(left_cmd, right_cmd, 0U);
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
    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL &&
        huart->Instance == LF_PORT_SENSOR_UART_HANDLE.Instance) {
        LF_SensorUart_OnByte(huart, (uint8_t)s_lf_sensor_uart_rx_byte);
        (void)HAL_UART_Receive_IT(huart, (uint8_t *)&s_lf_sensor_uart_rx_byte, 1U);
        return;
    }
    LF_RadarUart_RxCpltCallback(huart);
}

void LF_Port_UartErrorCallback(UART_HandleTypeDef *huart)
{
    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL &&
        huart->Instance == LF_PORT_SENSOR_UART_HANDLE.Instance) {
        LF_SensorUart_RecordUartError();
        (void)HAL_UART_Receive_IT(huart, (uint8_t *)&s_lf_sensor_uart_rx_byte, 1U);
        return;
    }
    LF_RadarUart_ErrorCallback(huart);
}

/* UART 发送完成回调：传感器串口清除忙标志，调试串口解锁发送缓冲。 */
void LF_Port_UartTxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }

    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL &&
        huart->Instance == LF_PORT_SENSOR_UART_HANDLE.Instance) {
        LF_SensorUart_OnTxComplete();
        return;
    }

#if LF_PORT_ENABLE_DEBUG_UART
    if (huart != NULL && huart->Instance == LF_PORT_DEBUG_UART_HANDLE.Instance) {
        s_lf_debug_tx_busy = false;
    }
#endif
}

#endif /* LF_USE_STM32F4_HAL_PORT */
