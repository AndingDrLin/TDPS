#ifndef BOARD_LORA_H
#define BOARD_LORA_H

#include "main.h"

void BoardLora_Init(void);
void BoardLora_Task(void);
void BoardLora_OnRxByte(uint8_t byte);
void BoardLora_OnError(void);
void BoardLora_PrintStatus(void);
void BoardLora_Reset(void);
void BoardLora_SetWake(uint8_t level);
void BoardLora_SetRaw(uint8_t enabled);
HAL_StatusTypeDef BoardLora_SendString(const char *text);
uint32_t BoardLora_GetRxCount(void);
uint32_t BoardLora_GetErrorCount(void);

#endif
