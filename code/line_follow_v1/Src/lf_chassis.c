#include "lf_chassis.h"

#include <stdint.h>

#include "lf_config.h"
#include "lf_platform.h"

static int16_t clamp_i16(int32_t v, int16_t lo, int16_t hi)
{
    if (v < (int32_t)lo) {
        return lo;
    }
    if (v > (int32_t)hi) {
        return hi;
    }
    return (int16_t)v;
}

static int16_t apply_deadband(int16_t cmd)
{
    if (cmd == 0) {
        return 0;
    }
    if (cmd > 0 && cmd < g_lf_config.motor_deadband) {
        return g_lf_config.motor_deadband;
    }
    if (cmd < 0 && cmd > -g_lf_config.motor_deadband) {
        return (int16_t)(-g_lf_config.motor_deadband);
    }
    return cmd;
}

void LF_Chassis_Init(void)
{
    LF_Platform_SetMotorCommand(0, 0);
}

void LF_Chassis_SetCommand(int16_t left_cmd, int16_t right_cmd)
{
    int16_t left = clamp_i16(left_cmd, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    int16_t right = clamp_i16(right_cmd, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);

    left = apply_deadband(left);
    right = apply_deadband(right);

    LF_Platform_SetMotorCommand(left, right);
}

void LF_Chassis_Stop(void)
{
    LF_Platform_SetMotorCommand(0, 0);
}
