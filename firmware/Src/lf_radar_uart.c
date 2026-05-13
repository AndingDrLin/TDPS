#ifdef LF_USE_STM32F4_HAL_PORT

#include "lf_radar_uart.h"

#include <stddef.h>

#include "lf_port_stm32f4_hal.h"

static volatile uint8_t s_rx_buf[LF_PORT_RADAR_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;
static volatile uint8_t s_rx_byte = 0U;

static void radar_rx_start(void)
{
    if (HAL_UART_Receive_IT(&LF_PORT_RADAR_UART_HANDLE, (uint8_t *)&s_rx_byte, 1U) != HAL_OK) {
        HAL_UART_AbortReceive_IT(&LF_PORT_RADAR_UART_HANDLE);
        (void)HAL_UART_Receive_IT(&LF_PORT_RADAR_UART_HANDLE, (uint8_t *)&s_rx_byte, 1U);
    }
}

void LF_RadarUart_Init(void)
{
    s_rx_head = 0U;
    s_rx_tail = 0U;
    HAL_NVIC_SetPriority(LF_PORT_RADAR_UART_IRQN,
                         LF_PORT_RADAR_UART_IRQ_PRIORITY,
                         LF_PORT_RADAR_UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(LF_PORT_RADAR_UART_IRQN);
    radar_rx_start();
}

uint16_t LF_RadarUart_Read(uint8_t *out_buf, uint16_t max_len)
{
    uint16_t count = 0U;

    if (out_buf == NULL) {
        return 0U;
    }

    while (count < max_len && s_rx_tail != s_rx_head) {
        out_buf[count++] = s_rx_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) % LF_PORT_RADAR_RX_BUFFER_SIZE);
    }

    return count;
}

void LF_RadarUart_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == LF_PORT_RADAR_UART_HANDLE.Instance) {
        uint16_t next = (uint16_t)((s_rx_head + 1U) % LF_PORT_RADAR_RX_BUFFER_SIZE);
        if (next != s_rx_tail) {
            s_rx_buf[s_rx_head] = s_rx_byte;
            s_rx_head = next;
        }
        radar_rx_start();
    }
}

void LF_RadarUart_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == LF_PORT_RADAR_UART_HANDLE.Instance) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        radar_rx_start();
    }
}

#endif /* LF_USE_STM32F4_HAL_PORT */
