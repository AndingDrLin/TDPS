#ifndef BOARD_MOTOR_H
#define BOARD_MOTOR_H

#include "main.h"

void BoardMotor_Init(void);
void BoardMotor_Task(void);
void BoardMotor_StopAll(void);
void BoardMotor_Lock(void);
void BoardMotor_Unlock(void);
uint8_t BoardMotor_IsLocked(void);
uint8_t BoardMotor_IsActive(void);
void BoardMotor_SetMaxDuty(int max_duty);
int BoardMotor_GetMaxDuty(void);
int BoardMotor_GetDutyA(void);
int BoardMotor_GetDutyB(void);
HAL_StatusTypeDef BoardMotor_Run(int duty_a, int duty_b, uint32_t duration_ms);
void BoardMotor_PrintStatus(void);

#endif
