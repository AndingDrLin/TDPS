#ifdef LF_USE_STM32F4_HAL_PORT

#include <string.h>

#include "main.h"
#include "lf_port_stm32f4_hal.h"
#include "lf_sensor_uart.h"
#include "wl_config.h"

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim10;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
volatile uint32_t g_lf_sensor_dma_buffer[LF_SENSOR_COUNT];

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

static void init_gpio(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    HAL_GPIO_WritePin(LF_PORT_LEFT_DIR_GPIO_PORT, LF_PORT_LEFT_DIR_PIN, LF_PORT_LEFT_FORWARD_LEVEL);
    HAL_GPIO_WritePin(LF_PORT_RIGHT_DIR_GPIO_PORT, LF_PORT_RIGHT_DIR_PIN, LF_PORT_RIGHT_FORWARD_LEVEL);
    HAL_GPIO_WritePin(LF_PORT_LED_GPIO_PORT, LF_PORT_LED_PIN,
                      (LF_PORT_LED_ON_LEVEL == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    gpio.Pin = LF_PORT_LEFT_DIR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LF_PORT_LEFT_DIR_GPIO_PORT, &gpio);

    gpio.Pin = LF_PORT_RIGHT_DIR_PIN;
    HAL_GPIO_Init(LF_PORT_RIGHT_DIR_GPIO_PORT, &gpio);

    gpio.Pin = LF_PORT_LED_PIN;
    HAL_GPIO_Init(LF_PORT_LED_GPIO_PORT, &gpio);

    gpio.Pin = LF_PORT_BTN_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LF_PORT_BTN_GPIO_PORT, &gpio);

#if defined(LF_PORT_FRONT_AUX_LEFT_PIN) && defined(LF_PORT_FRONT_AUX_LEFT_PORT)
    gpio.Pin = LF_PORT_FRONT_AUX_LEFT_PIN;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LF_PORT_FRONT_AUX_LEFT_PORT, &gpio);
#endif

#if defined(LF_PORT_FRONT_AUX_RIGHT_PIN) && defined(LF_PORT_FRONT_AUX_RIGHT_PORT)
    gpio.Pin = LF_PORT_FRONT_AUX_RIGHT_PIN;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LF_PORT_FRONT_AUX_RIGHT_PORT, &gpio);
#endif
}

static void init_adc1(void)
{
    ADC_ChannelConfTypeDef config = {0};
    uint32_t channel;
    uint32_t rank;

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = LF_SENSOR_COUNT;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    for (rank = 1U; rank <= LF_SENSOR_COUNT; ++rank) {
        channel = ADC_CHANNEL_0 + ((rank - 1U) << ADC_CR1_AWDCH_Pos);
        config.Channel = channel;
        config.Rank = rank;
        config.SamplingTime = ADC_SAMPLETIME_84CYCLES;
        if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK) {
            Error_Handler();
        }
    }
}

static void init_motor_pwm(void)
{
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef oc = {0};

    htim8.Instance = TIM8;
    htim8.Init.Prescaler = 15U;
    htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim8.Init.Period = 999U;
    htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim8.Init.RepetitionCounter = 0U;
    htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim8) != HAL_OK) {
        Error_Handler();
    }

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &master) != HAL_OK) {
        Error_Handler();
    }

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim8, &oc, LF_PORT_LEFT_PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_MspPostInit(&htim8);

    htim10.Instance = TIM10;
    htim10.Init.Prescaler = 15U;
    htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim10.Init.Period = 999U;
    htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim10) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_ConfigChannel(&htim10, &oc, LF_PORT_RIGHT_PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_MspPostInit(&htim10);
}

static void init_uart(UART_HandleTypeDef *huart, USART_TypeDef *instance, uint32_t baudrate)
{
    huart->Instance = instance;
    huart->Init.BaudRate = baudrate;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(huart) != HAL_OK) {
        Error_Handler();
    }
}

void LF_Port_SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

bool LF_Port_ReadLineSensorDigital(uint8_t out_level[LF_SENSOR_COUNT])
{
    if (out_level == NULL) {
        return false;
    }

    memset(out_level, 0, LF_SENSOR_COUNT);
    out_level[0] = (HAL_GPIO_ReadPin(LF_PORT_FRONT_AUX_LEFT_PORT, LF_PORT_FRONT_AUX_LEFT_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    out_level[1] = (HAL_GPIO_ReadPin(LF_PORT_FRONT_AUX_RIGHT_PORT, LF_PORT_FRONT_AUX_RIGHT_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    return true;
}

void LF_Port_Peripheral_Init(void)
{
    init_gpio();
    if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_ANALOG_ADC) {
        init_adc1();
    }
    init_motor_pwm();
    init_uart(&huart2, USART2, LF_SENSOR_UART_BAUDRATE);
    init_uart(&huart3, USART3, g_lf_config.radar_uart_baudrate);
}

#endif /* LF_USE_STM32F4_HAL_PORT */
