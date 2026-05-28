#ifndef BOARD_TEST_H
#define BOARD_TEST_H

#include "main.h"

void BoardTest_Init(void);
void BoardTest_Task(void);
void BoardTest_PrintStatus(void);
void BoardTest_PrintPins(void);
void BoardTest_PrintGpio(void);
void BoardTest_SelfTestPassive(void);
void BoardTest_SelfTestMotorSafe(void);

#endif
