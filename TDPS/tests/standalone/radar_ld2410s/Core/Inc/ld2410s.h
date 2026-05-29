#ifndef __LD2410S_H
#define __LD2410S_H

#include "main.h"

typedef enum
{
    LD2410S_FRAME_NONE = 0,
    LD2410S_FRAME_MINIMAL = 1,
    LD2410S_FRAME_STANDARD = 2
} LD2410S_FrameType;

typedef struct
{
    LD2410S_FrameType frame_type;
    uint8_t target_state;
    uint16_t distance_cm;
    uint32_t gate_energy[16];
    uint8_t strongest_gate;
} LD2410S_FrameInfo;

void LD2410S_Init(UART_HandleTypeDef *radar_huart, UART_HandleTypeDef *debug_huart);
void LD2410S_StartReceiveIT(void);
void LD2410S_RxCpltCallback(UART_HandleTypeDef *huart);
void LD2410S_ErrorCallback(UART_HandleTypeDef *huart);
uint8_t LD2410S_GetFrame(LD2410S_FrameInfo *info);
uint32_t LD2410S_FrameCount(void);
uint32_t LD2410S_RawRxBytes(void);
uint32_t LD2410S_ParseErrorCount(void);
uint32_t LD2410S_RxOverflowCount(void);
HAL_StatusTypeDef LD2410S_LastRxStatus(void);
void LD2410S_SwitchToStandardMode(void);
const char *LD2410S_TargetStateString(uint8_t state);
const char *LD2410S_GateRangeString(uint8_t gate);
void Debug_Printf(const char *fmt, ...);

#endif /* __LD2410S_H */
