/**
 * @file    wl_platform_stm32f1.c
 * @brief   STM32F103C8T6 HAL 平台抽象层实现。
 *
 * 仅在定义了 WL_USE_STM32F1_HAL_PORT 宏时编译。
 * 提供 wl_platform.h 中声明的所有接口的真实硬件实现。
 */

#ifdef WL_USE_STM32F1_HAL_PORT

#include "wl_platform.h"
#include "wl_port_stm32f1.h"
#include "wl_config.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  HAL 句柄定义                                                       */
/* ------------------------------------------------------------------ */

UART_HandleTypeDef wl_huart;        /* 与 EWM22A 模块通信的 UART */
UART_HandleTypeDef wl_huart_debug;  /* 调试串口（可选） */

/* ------------------------------------------------------------------ */
/*  UART 接收环形缓冲区                                                */
/* ------------------------------------------------------------------ */

static volatile uint8_t  rx_buf[WL_UART_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;   /* 中断写入位置 */
static volatile uint16_t rx_tail = 0;   /* 应用读取位置 */

/** 单字节中断接收临时变量。 */
static volatile uint8_t  rx_byte;

/* ------------------------------------------------------------------ */
/*  内部辅助函数                                                       */
/* ------------------------------------------------------------------ */

/** 初始化与 EWM22A 模块通信的 UART 外设。 */
static void _uart_init(void)
{
    /* 使能 USART2 和 GPIOA 时钟 */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 配置 TX 引脚：复用推挽输出 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = WL_UART_TX_PIN;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(WL_UART_TX_PORT, &gpio);

    /* 配置 RX 引脚：浮空输入 */
    gpio.Pin  = WL_UART_RX_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(WL_UART_RX_PORT, &gpio);

    /* 配置 UART 参数 */
    wl_huart.Instance          = WL_UART_INSTANCE;
    wl_huart.Init.BaudRate     = WL_UART_BAUDRATE;
    wl_huart.Init.WordLength   = UART_WORDLENGTH_8B;
    wl_huart.Init.StopBits     = UART_STOPBITS_1;
    wl_huart.Init.Parity       = UART_PARITY_NONE;
    wl_huart.Init.Mode         = UART_MODE_TX_RX;
    wl_huart.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    wl_huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&wl_huart);

    /* 使能 UART 接收中断 */
    HAL_NVIC_SetPriority(USART2_IRQn,
                         WL_UART_IRQ_PRIORITY,
                         WL_UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    /* 启动第一次中断接收 */
    HAL_UART_Receive_IT(&wl_huart, (uint8_t *)&rx_byte, 1);
}

/** 初始化 AUX 引脚为输入模式。 */
static void _aux_gpio_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = WL_AUX_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;    /* AUX 默认高电平，加上拉 */
    HAL_GPIO_Init(WL_AUX_PORT, &gpio);
}

/** 初始化调试串口（可选，使用 USART1）。 */
static void _debug_uart_init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = WL_DEBUG_TX_PIN;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(WL_DEBUG_TX_PORT, &gpio);

    gpio.Pin  = WL_DEBUG_RX_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(WL_DEBUG_RX_PORT, &gpio);

    wl_huart_debug.Instance          = WL_DEBUG_UART_INSTANCE;
    wl_huart_debug.Init.BaudRate     = 115200;
    wl_huart_debug.Init.WordLength   = UART_WORDLENGTH_8B;
    wl_huart_debug.Init.StopBits     = UART_STOPBITS_1;
    wl_huart_debug.Init.Parity       = UART_PARITY_NONE;
    wl_huart_debug.Init.Mode         = UART_MODE_TX_RX;
    wl_huart_debug.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    wl_huart_debug.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&wl_huart_debug);
}

/* ------------------------------------------------------------------ */
/*  UART 接收中断回调                                                   */
/* ------------------------------------------------------------------ */

/**
 * HAL 库 UART 接收完成回调。
 * 每收到一个字节，将其写入环形缓冲区，然后重新启动接收。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == WL_UART_INSTANCE) {
        uint16_t next = (rx_head + 1) % WL_UART_RX_BUF_SIZE;
        if (next != rx_tail) {          /* 缓冲区未满 */
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        /* 重新启动单字节中断接收 */
        HAL_UART_Receive_IT(&wl_huart, (uint8_t *)&rx_byte, 1);
    }
}

/**
 * USART2 中断服务函数。
 * 需要在 stm32f1xx_it.c 中调用，或直接在此处定义（弱符号覆盖）。
 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&wl_huart);
}

/* ------------------------------------------------------------------ */
/*  PAL 接口实现                                                       */
/* ------------------------------------------------------------------ */

void WL_Platform_Init(void)
{
    _aux_gpio_init();
    _uart_init();
    _debug_uart_init();
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
    HAL_UART_Transmit(&wl_huart, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void WL_Platform_UART_SendString(const char *str)
{
    WL_Platform_UART_Send((const uint8_t *)str, (uint16_t)strlen(str));
}

uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len)
{
    uint16_t count = 0;
    while (count < max_len && rx_tail != rx_head) {
        buf[count++] = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % WL_UART_RX_BUF_SIZE;
    }
    return count;
}

void WL_Platform_UART_FlushRx(void)
{
    rx_tail = rx_head;
}

bool WL_Platform_ReadAUX(void)
{
    return (HAL_GPIO_ReadPin(WL_AUX_PORT, WL_AUX_PIN) == GPIO_PIN_SET);
}

void WL_Platform_DebugPrint(const char *msg)
{
    HAL_UART_Transmit(&wl_huart_debug,
                      (uint8_t *)msg,
                      (uint16_t)strlen(msg),
                      HAL_MAX_DELAY);
}

#endif /* WL_USE_STM32F1_HAL_PORT */
