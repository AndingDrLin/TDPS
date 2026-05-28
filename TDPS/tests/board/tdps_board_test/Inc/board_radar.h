#ifndef BOARD_RADAR_H
#define BOARD_RADAR_H

#include "main.h"

void BoardRadar_Init(void);
void BoardRadar_Task(void);
void BoardRadar_OnRxByte(uint8_t byte);
void BoardRadar_OnError(void);
void BoardRadar_SetRaw(uint8_t enabled);
void BoardRadar_SetParse(uint8_t enabled);
void BoardRadar_PrintStatus(void);
void BoardRadar_PrintGpio(void);
uint32_t BoardRadar_GetRxCount(void);
uint32_t BoardRadar_GetErrorCount(void);
uint8_t BoardRadar_GetTargetState(void);
uint16_t BoardRadar_GetDistanceCm(void);
uint32_t BoardRadar_GetLastFrameTick(void);

#endif
