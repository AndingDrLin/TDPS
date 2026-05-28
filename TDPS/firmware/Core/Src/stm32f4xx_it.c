/*
 * TDPS Firmware — Interrupt Handlers
 *
 * 中断服务函数：
 * - Cortex-M4 系统异常
 * - USART2 IRQ → 巡线传感器 UART
 * - USART3 IRQ → 雷达 UART
 * - DMA2 Stream0 IRQ → ADC1 DMA（模拟传感器模式）
 *
 * HAL 回调函数在 tdps_uart_callbacks.c 中实现，
 * 通过 LF_Port_Uart*Callback 转发到各模块。
 */

#include "stm32f4xx_it.h"
#include "main.h"

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_adc1;

/* ===== Cortex-M4 System Exceptions ===== */

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ===== Peripheral Interrupt Handlers ===== */

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}
