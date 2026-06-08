/**
 * @file lf_ultrasonic_stm32f4.c
 * @brief STM32F4 HAL platform implementation for the ultrasonic sensor.
 *
 * Pin assignments (HC-SR04 compatible):
 * - TRIG: PD3  (GPIO output, push-pull)
 * - ECHO: PD4  (GPIO input)
 *
 * Timing uses DWT cycle counter for microsecond precision.
 */
#include "lf_ultrasonic_platform.h"

#ifdef LF_USE_STM32F4_HAL_PORT

#include "stm32f4xx_hal.h"

/* TRIG pin: PD3 */
#define US_TRIG_PORT    GPIOD
#define US_TRIG_PIN     GPIO_PIN_3

/* ECHO pin: PD4 */
#define US_ECHO_PORT    GPIOD
#define US_ECHO_PIN     GPIO_PIN_4

/**
 * @brief Enable the DWT cycle counter for microsecond timing.
 */
static void enable_dwt_cycle_counter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void LF_Ultrasonic_Platform_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable GPIOD clock */
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Configure TRIG pin as push-pull output, default low */
    gpio.Pin = US_TRIG_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(US_TRIG_PORT, &gpio);
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);

    /* Configure ECHO pin as input */
    gpio.Pin = US_ECHO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(US_ECHO_PORT, &gpio);

    /* Enable DWT for microsecond timing */
    enable_dwt_cycle_counter();
}

void LF_Ultrasonic_Platform_Trigger(void)
{
    /* Send a 10us trigger pulse per HC-SR04 spec. */
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_SET);

    /* Busy-wait 10us using DWT cycle counter. */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * 10U;
    while ((DWT->CYCCNT - start) < ticks) {
        /* spin */
    }

    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);
}

bool LF_Ultrasonic_Platform_ReadEcho(void)
{
    return HAL_GPIO_ReadPin(US_ECHO_PORT, US_ECHO_PIN) == GPIO_PIN_SET;
}

void LF_Ultrasonic_Platform_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * us;
    while ((DWT->CYCCNT - start) < ticks) {
        /* spin */
    }
}

uint32_t LF_Ultrasonic_Platform_GetMicros(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

#endif /* LF_USE_STM32F4_HAL_PORT */
