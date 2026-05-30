#include "main.h"

#include <string.h>

#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_platform.h"
#include "lf_sensor.h"
#include "lf_sensor_uart.h"

UART_HandleTypeDef huart2;
volatile uint8_t g_line_uart_rx_byte;
volatile LineSensorUartWatchInfo g_line_watch;
LF_SensorFrame g_line_frame;
LF_PIDState g_line_pid;
volatile int16_t g_line_motor_left_applied;
volatile int16_t g_line_motor_right_applied;

static int16_t g_line_motor_left_raw;
static int16_t g_line_motor_right_raw;
static uint32_t g_last_control_tick;
static uint8_t g_uart_command_sent;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void ConfigureLineTest(void);
static void StartLineUart(void);
static void UpdateAuxWatch(void);
static void UpdateLineWatch(float error, float dt_s, int16_t correction, uint8_t command_valid);
static void SetStatusLed(uint8_t on);

static uint8_t AuxIsActive(uint8_t level)
{
#if LINE_AUX_ACTIVE_HIGH
    return level ? 1U : 0U;
#else
    return level ? 0U : 1U;
#endif
}

int main(void)
{
    uint32_t now;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    ConfigureLineTest();
    LF_SensorUart_Init(&huart2);
    StartLineUart();

    HAL_Delay(1000U);
    if (LF_SensorUart_SendCommand(LF_SENSOR_UART_COMMAND_BOTH) == HAL_OK) {
        g_uart_command_sent = 1U;
    } else {
        g_line_watch.fault_flags |= 0x01U;
    }

    LF_Sensor_Init();
    LF_Sensor_StartCalibration();
    LF_Sensor_EndCalibration();
    LF_Control_ResetPid(&g_line_pid);
    LF_Chassis_Init();

    g_last_control_tick = HAL_GetTick();
    g_line_watch.initialized = 1U;
    SetStatusLed(1U);

    while (1) {
        now = HAL_GetTick();
        if ((now - g_last_control_tick) >= g_lf_config.control_period_ms) {
            float dt_s = (float)(now - g_last_control_tick) / 1000.0f;
            float error = 0.0f;
            int16_t correction = 0;
            uint8_t command_valid = 0U;

            g_last_control_tick = now;
            UpdateAuxWatch();
            LF_Sensor_ReadFrame(&g_line_frame);

            if (g_line_frame.line_detected) {
                error = (float)(-g_line_frame.position);
                correction = LF_Control_UpdatePid(error, dt_s, &g_line_pid);
                LF_Control_ComputeMotorCmd(g_lf_config.base_speed,
                                           correction,
                                           &g_line_motor_left_raw,
                                           &g_line_motor_right_raw);
                LF_Chassis_SetCommand(g_line_motor_left_raw, g_line_motor_right_raw);
                command_valid = 1U;
            } else {
                g_line_motor_left_raw = 0;
                g_line_motor_right_raw = 0;
                LF_Chassis_Stop();
            }

            UpdateLineWatch(error, dt_s, correction, command_valid);
        }

        if (g_uart_command_sent == 0U && HAL_GetTick() > 1500U) {
            if (LF_SensorUart_SendCommand(LF_SENSOR_UART_COMMAND_BOTH) == HAL_OK) {
                g_uart_command_sent = 1U;
                g_line_watch.fault_flags &= (uint8_t)~0x01U;
            }
        }

        HAL_Delay(1U);
    }
}

static void ConfigureLineTest(void)
{
    g_lf_config.sensor_input_mode = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration = true;
    g_lf_config.sensor_invert_polarity = false;
    g_lf_config.sensor_digital_active_high = false;
    g_lf_config.sensor_filter_alpha = 0.60f;
    g_lf_config.line_detect_min_sum = 780U;
    g_lf_config.line_detect_min_peak = 260U;
    g_lf_config.line_detect_min_contrast = 80U;
    g_lf_config.radar_enable = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable = false;
    g_lf_config.base_speed = 180;
    g_lf_config.max_correction = 250;
    g_lf_config.max_motor_cmd = 600;
    g_lf_config.motor_deadband = 0;
}

static void StartLineUart(void)
{
    if (HAL_UART_Receive_IT(&huart2, (uint8_t *)&g_line_uart_rx_byte, 1U) != HAL_OK) {
        g_line_watch.fault_flags |= 0x02U;
    }
}

static void UpdateAuxWatch(void)
{
    uint8_t level0 = (HAL_GPIO_ReadPin(LINE_AUX1_GPIO_Port, LINE_AUX1_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    uint8_t level1 = (HAL_GPIO_ReadPin(LINE_AUX2_GPIO_Port, LINE_AUX2_Pin) == GPIO_PIN_SET) ? 1U : 0U;

    g_line_watch.aux_gpio_level[0] = level0;
    g_line_watch.aux_gpio_level[1] = level1;
    g_line_watch.aux_active[0] = AuxIsActive(level0);
    g_line_watch.aux_active[1] = AuxIsActive(level1);
    g_line_watch.aux_level_mask = (uint8_t)(level0 | (uint8_t)(level1 << 1));
    g_line_watch.aux_active_mask = (uint8_t)(g_line_watch.aux_active[0] |
                                             (uint8_t)(g_line_watch.aux_active[1] << 1));
}

static void UpdateLineWatch(float error, float dt_s, int16_t correction, uint8_t command_valid)
{
    LF_SensorUartStats stats;
    const LF_SensorCalibration *calib;
    uint16_t analog[LF_SENSOR_COUNT];
    uint8_t digital[LF_SENSOR_COUNT];
    uint32_t i;
    uint32_t now = HAL_GetTick();

    memset(analog, 0, sizeof(analog));
    memset(digital, 0, sizeof(digital));
    (void)LF_SensorUart_GetAnalogFrame(analog);
    (void)LF_SensorUart_GetDigitalFrame(digital);
    LF_SensorUart_GetStats(&stats);
    calib = LF_Sensor_GetCalibration();

    g_line_watch.tick_ms = now;
    g_line_watch.loop_count++;
    g_line_watch.sample_count++;
    g_line_watch.last_update_ms = now;
    g_line_watch.rx_byte_count = stats.rx_byte_count;
    g_line_watch.analog_frame_count = stats.analog_frame_count;
    g_line_watch.digital_frame_count = stats.digital_frame_count;
    g_line_watch.parse_error_count = stats.parse_error_count;
    g_line_watch.uart_error_count = stats.uart_error_count;
    g_line_watch.last_frame_age_ms = (stats.last_frame_ms == 0U) ? 0xFFFFFFFFU : (now - stats.last_frame_ms);
    g_line_watch.last_rx_byte = stats.last_rx_byte;
    LF_SensorUart_GetLastAsciiFrame((char *)g_line_watch.last_ascii_frame);

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        g_line_watch.uart_analog[i] = analog[i];
        g_line_watch.uart_digital[i] = digital[i];
        g_line_watch.main_raw[i] = g_line_frame.raw[i];
        g_line_watch.norm[i] = g_line_frame.norm[i];
        g_line_watch.filtered_u16[i] = g_line_frame.filtered_u16[i];
    }

    g_line_watch.signal_sum = g_line_frame.signal_sum;
    g_line_watch.peak_value = g_line_frame.peak_value;
    g_line_watch.contrast_value = g_line_frame.contrast_value;
    g_line_watch.peak_index = g_line_frame.peak_index;
    g_line_watch.active_count = g_line_frame.active_count;
    g_line_watch.line_confidence = g_line_frame.line_confidence;
    g_line_watch.edge_hint = g_line_frame.edge_hint;
    g_line_watch.position = g_line_frame.position;
    g_line_watch.line_detected = g_line_frame.line_detected ? 1U : 0U;

    g_line_watch.error = error;
    g_line_watch.dt_s = dt_s;
    g_line_watch.correction = correction;
    g_line_watch.base_speed = g_lf_config.base_speed;
    g_line_watch.motor_left_raw = g_line_motor_left_raw;
    g_line_watch.motor_right_raw = g_line_motor_right_raw;
    g_line_watch.motor_left_applied = g_line_motor_left_applied;
    g_line_watch.motor_right_applied = g_line_motor_right_applied;
    g_line_watch.motor_command_valid = command_valid;

    g_line_watch.sensor_input_mode = (uint8_t)g_lf_config.sensor_input_mode;
    g_line_watch.sensor_invert_polarity = g_lf_config.sensor_invert_polarity ? 1U : 0U;
    g_line_watch.sensor_digital_active_high = g_lf_config.sensor_digital_active_high ? 1U : 0U;
    g_line_watch.sensor_filter_alpha_x100 = (uint16_t)(g_lf_config.sensor_filter_alpha * 100.0f);
    g_line_watch.line_detect_min_sum = g_lf_config.line_detect_min_sum;
    g_line_watch.line_detect_min_peak = g_lf_config.line_detect_min_peak;
    g_line_watch.line_detect_min_contrast = g_lf_config.line_detect_min_contrast;
    g_line_watch.calibration_status = (uint8_t)calib->status;
    g_line_watch.calibration_done = calib->calibrated ? 1U : 0U;
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = LF_SENSOR_UART_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LINE_STATUS_LED_GPIO_Port, LINE_STATUS_LED_Pin,
                      (LINE_STATUS_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LINE_AUX1_Pin | LINE_AUX2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LINE_STATUS_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LINE_STATUS_LED_GPIO_Port, &GPIO_InitStruct);
}

static void SetStatusLed(uint8_t on)
{
    GPIO_PinState level = on ? LINE_STATUS_LED_ON_LEVEL :
                          ((LINE_STATUS_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LINE_STATUS_LED_GPIO_Port, LINE_STATUS_LED_Pin, level);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        LF_SensorUart_OnByte(huart, g_line_uart_rx_byte);
        if (HAL_UART_Receive_IT(huart, (uint8_t *)&g_line_uart_rx_byte, 1U) != HAL_OK) {
            g_line_watch.fault_flags |= 0x02U;
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        LF_SensorUart_RecordUartError();
        if (HAL_UART_Receive_IT(huart, (uint8_t *)&g_line_uart_rx_byte, 1U) != HAL_OK) {
            g_line_watch.fault_flags |= 0x02U;
        }
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
