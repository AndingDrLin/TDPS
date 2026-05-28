#ifndef BOARD_LINE_H
#define BOARD_LINE_H

#include "main.h"

void BoardLine_Init(void);
void BoardLine_Task(void);
void BoardLine_OnRxByte(uint8_t byte);
void BoardLine_OnError(void);
void BoardLine_PrintStatus(void);
void BoardLine_PrintIo(void);
void BoardLine_SetRaw(uint8_t enabled);
HAL_StatusTypeDef BoardLine_SendString(const char *text);
uint32_t BoardLine_GetRxCount(void);
uint32_t BoardLine_GetErrorCount(void);
uint32_t BoardLine_GetLastRxTick(void);

#endif
