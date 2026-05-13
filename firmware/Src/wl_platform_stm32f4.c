#ifdef WL_USE_STM32F4_HAL_PORT

#include "wl_platform.h"

#include <stddef.h>
#include <string.h>

#include "wl_config.h"
#include "wl_port_stm32f4.h"

UART_HandleTypeDef wl_huart;
UART_HandleTypeDef wl_huart_debug;

static volatile uint8_t rx_buf[WL_UART_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static volatile uint8_t rx_byte;
static volatile bool s_debug_tx_busy = false;

static void enable_uart_clock(USART_TypeDef *instance)
{
    if (instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
    } else if (instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
    } else if (instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
    }
}

static void gpio_clock_enable(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
}

static void uart_rx_start(void)
{
    if (HAL_UART_Receive_IT(&wl_huart, (uint8_t *)&rx_byte, 1U) != HAL_OK) {
        HAL_UART_AbortReceive_IT(&wl_huart);
        (void)HAL_UART_Receive_IT(&wl_huart, (uint8_t *)&rx_byte, 1U);
    }
}

static void uart_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio_clock_enable(WL_UART_TX_PORT);
    gpio_clock_enable(WL_UART_RX_PORT);

    gpio.Pin = WL_UART_TX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = WL_UART_TX_AF;
    HAL_GPIO_Init(WL_UART_TX_PORT, &gpio);

    gpio.Pin = WL_UART_RX_PIN;
    gpio.Alternate = WL_UART_RX_AF;
    HAL_GPIO_Init(WL_UART_RX_PORT, &gpio);
}

static void uart_init(void)
{
    enable_uart_clock(WL_UART_INSTANCE);
    uart_gpio_init();

    wl_huart.Instance = WL_UART_INSTANCE;
    wl_huart.Init.BaudRate = WL_UART_BAUDRATE;
    wl_huart.Init.WordLength = UART_WORDLENGTH_8B;
    wl_huart.Init.StopBits = UART_STOPBITS_1;
    wl_huart.Init.Parity = UART_PARITY_NONE;
    wl_huart.Init.Mode = UART_MODE_TX_RX;
    wl_huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wl_huart.Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(&wl_huart);

    HAL_NVIC_SetPriority(WL_UART_IRQN, WL_UART_IRQ_PRIORITY, WL_UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(WL_UART_IRQN);
    uart_rx_start();
}

static void aux_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio_clock_enable(WL_AUX_PORT);
    gpio.Pin = WL_AUX_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(WL_AUX_PORT, &gpio);
}

static void debug_uart_init(void)
{
#if WL_ENABLE_DEBUG_UART
    GPIO_InitTypeDef gpio = {0};

    enable_uart_clock(WL_DEBUG_UART_INSTANCE);
    gpio_clock_enable(WL_DEBUG_TX_PORT);
    gpio_clock_enable(WL_DEBUG_RX_PORT);

    gpio.Pin = WL_DEBUG_TX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = WL_DEBUG_TX_AF;
    HAL_GPIO_Init(WL_DEBUG_TX_PORT, &gpio);

    gpio.Pin = WL_DEBUG_RX_PIN;
    gpio.Alternate = WL_DEBUG_RX_AF;
    HAL_GPIO_Init(WL_DEBUG_RX_PORT, &gpio);

    wl_huart_debug.Instance = WL_DEBUG_UART_INSTANCE;
    wl_huart_debug.Init.BaudRate = 115200U;
    wl_huart_debug.Init.WordLength = UART_WORDLENGTH_8B;
    wl_huart_debug.Init.StopBits = UART_STOPBITS_1;
    wl_huart_debug.Init.Parity = UART_PARITY_NONE;
    wl_huart_debug.Init.Mode = UART_MODE_TX_RX;
    wl_huart_debug.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    wl_huart_debug.Init.OverSampling = UART_OVERSAMPLING_16;
    (void)HAL_UART_Init(&wl_huart_debug);
#endif
}

void WL_Port_UartRxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == WL_UART_INSTANCE) {
        uint16_t next = (uint16_t)((rx_head + 1U) % WL_UART_RX_BUF_SIZE);
        if (next != rx_tail) {
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        uart_rx_start();
    }
}

void WL_Port_UartErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == WL_UART_INSTANCE) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        uart_rx_start();
    }
}

void WL_Port_UartTxCpltCallback(UART_HandleTypeDef *huart)
{
#if WL_ENABLE_DEBUG_UART
    if (huart != NULL && huart->Instance == WL_DEBUG_UART_INSTANCE) {
        s_debug_tx_busy = false;
    }
#else
    (void)huart;
#endif
}

void WL_Platform_Init(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    s_debug_tx_busy = false;
    aux_gpio_init();
    uart_init();
    debug_uart_init();
}

uint32_t WL_Platform_GetMillis(void)
{
    return HAL_GetTick();
}

void WL_Platform_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void WL_Platform_UART_Send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }
    (void)HAL_UART_Transmit(&wl_huart, (uint8_t *)data, len, g_wl_config.tx_timeout_ms);
}

void WL_Platform_UART_SendString(const char *str)
{
    if (str == NULL) {
        return;
    }
    WL_Platform_UART_Send((const uint8_t *)str, (uint16_t)strlen(str));
}

uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len)
{
    uint16_t count = 0U;

    if (buf == NULL) {
        return 0U;
    }

    while (count < max_len && rx_tail != rx_head) {
        buf[count++] = rx_buf[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % WL_UART_RX_BUF_SIZE);
    }
    return count;
}

void WL_Platform_UART_FlushRx(void)
{
    rx_tail = rx_head;
}

bool WL_Platform_ReadAUX(void)
{
    return HAL_GPIO_ReadPin(WL_AUX_PORT, WL_AUX_PIN) == GPIO_PIN_SET;
}

void WL_Platform_DebugPrint(const char *msg)
{
#if WL_ENABLE_DEBUG_UART
    if (msg == NULL || s_debug_tx_busy) {
        return;
    }
    s_debug_tx_busy = true;
    if (HAL_UART_Transmit_IT(&wl_huart_debug, (uint8_t *)msg, (uint16_t)strlen(msg)) != HAL_OK) {
        s_debug_tx_busy = false;
    }
#else
    (void)msg;
#endif
}

#endif /* WL_USE_STM32F4_HAL_PORT */
