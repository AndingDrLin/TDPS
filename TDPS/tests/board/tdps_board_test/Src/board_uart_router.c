#include "board_uart_router.h"
#include "board_cli.h"
#include "board_lora.h"
#include "board_radar.h"
#include "board_line.h"

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart6;

static uint8_t s_cli_rx;
static uint8_t s_radar_rx;
static uint8_t s_lora_rx;
static uint8_t s_line_rx;

static void start_rx(UART_HandleTypeDef *huart, uint8_t *byte)
{
    (void)HAL_UART_Receive_IT(huart, byte, 1);
}

void BoardUartRouter_Init(void)
{
    start_rx(&huart6, &s_cli_rx);
    start_rx(&huart3, &s_radar_rx);
    start_rx(&huart5, &s_lora_rx);
    start_rx(&huart2, &s_line_rx);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        BoardCli_OnRxByte(s_cli_rx);
        start_rx(&huart6, &s_cli_rx);
    }
    else if (huart->Instance == USART3)
    {
        BoardRadar_OnRxByte(s_radar_rx);
        start_rx(&huart3, &s_radar_rx);
    }
    else if (huart->Instance == UART5)
    {
        BoardLora_OnRxByte(s_lora_rx);
        start_rx(&huart5, &s_lora_rx);
    }
    else if (huart->Instance == USART2)
    {
        BoardLine_OnRxByte(s_line_rx);
        start_rx(&huart2, &s_line_rx);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
        BoardCli_OnError();
        start_rx(&huart6, &s_cli_rx);
    }
    else if (huart->Instance == USART3)
    {
        BoardRadar_OnError();
        start_rx(&huart3, &s_radar_rx);
    }
    else if (huart->Instance == UART5)
    {
        BoardLora_OnError();
        start_rx(&huart5, &s_lora_rx);
    }
    else if (huart->Instance == USART2)
    {
        BoardLine_OnError();
        start_rx(&huart2, &s_line_rx);
    }
}
