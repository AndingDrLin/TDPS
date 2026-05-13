#ifndef LF_CHASSIS_H
#define LF_CHASSIS_H

#include <stdint.h>

/* 底盘模块初始化。 */
void LF_Chassis_Init(void);

/* 下发左右轮命令（-1000~1000）。 */
void LF_Chassis_SetCommand(int16_t left_cmd, int16_t right_cmd);

/* 紧急停车。 */
void LF_Chassis_Stop(void);

#endif /* LF_CHASSIS_H */
