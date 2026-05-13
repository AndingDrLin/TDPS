#if defined(LF_USE_STM32F4_HAL_PORT) && defined(WL_USE_STM32F4_HAL_PORT)

#include "lf_port_stm32f4_hal.h"
#include "wl_port_stm32f4.h"

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    LF_Port_UartRxCpltCallback(huart);
    WL_Port_UartRxCpltCallback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    LF_Port_UartErrorCallback(huart);
    WL_Port_UartErrorCallback(huart);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    LF_Port_UartTxCpltCallback(huart);
    WL_Port_UartTxCpltCallback(huart);
}

#endif
