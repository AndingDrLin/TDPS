#ifndef BOARD_ENCODER_H
#define BOARD_ENCODER_H

#include "main.h"

void BoardEncoder_Init(void);
void BoardEncoder_Zero(void);
int32_t BoardEncoder_ReadA(void);
int32_t BoardEncoder_ReadB(void);
void BoardEncoder_Print(void);
void BoardEncoder_Watch(uint32_t duration_ms);

#endif
