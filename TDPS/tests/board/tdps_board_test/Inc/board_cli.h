#ifndef BOARD_CLI_H
#define BOARD_CLI_H

#include "main.h"

void BoardCli_Init(void);
void BoardCli_Task(void);
void BoardCli_OnRxByte(uint8_t byte);
void BoardCli_OnError(void);
int Board_Printf(const char *fmt, ...);
HAL_StatusTypeDef Board_Write(const uint8_t *data, uint16_t len);

#endif
