/**
 * @file lf_chassis.c
 * @brief Differential-drive chassis motor command layer.
 */

#include "lf_chassis.h"

#include <stdint.h>

#include "lf_config.h"
#include "lf_platform.h"
#include "clamp.h"

/**
 * @brief Apply motor deadband compensation.
 *
 * Values between 0 and the deadband threshold are bumped to the threshold
 * to overcome static friction in the motor gearbox.
 *
 * @param cmd Raw motor command.
 * @return Command with deadband applied.
 */
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

void LF_Chassis_LimitMotorDelta(int16_t *left_cmd, int16_t *right_cmd, int16_t limit)
{
    int32_t left;
    int32_t right;
    int32_t delta;
    int32_t avg;

    if (left_cmd == 0 || right_cmd == 0 || limit <= 0) {
        return;
    }

    left = *left_cmd;
    right = *right_cmd;
    delta = left - right;
    if (delta <= limit && delta >= -limit) {
        return;
    }

    avg = (left + right) / 2;
    if (delta > 0) {
        left = avg + (limit / 2);
        right = avg - (limit / 2);
    } else {
        left = avg - (limit / 2);
        right = avg + (limit / 2);
    }

    *left_cmd = TDPS_ClampI16(left, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    *right_cmd = TDPS_ClampI16(right, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
}

/**
 * @brief Initialize the chassis module and stop both motors.
 */
void LF_Chassis_Init(void)
{
    LF_Platform_SetMotorCommand(0, 0);
}

/**
 * @brief Set left/right motor commands with clamping, deadband, and delta limiting.
 *
 * @param left_cmd  Desired left motor command.
 * @param right_cmd Desired right motor command.
 */
void LF_Chassis_SetCommand(int16_t left_cmd, int16_t right_cmd)
{
    int16_t left = TDPS_ClampI16(left_cmd, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    int16_t right = TDPS_ClampI16(right_cmd, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);

    left = apply_deadband(left);
    right = apply_deadband(right);
    LF_Chassis_LimitMotorDelta(&left, &right, g_lf_config.max_motor_delta);

    LF_Platform_SetMotorCommand(-left, -right);
}

/**
 * @brief Emergency stop: immediately set both motors to zero.
 */
void LF_Chassis_Stop(void)
{
    LF_Platform_SetMotorCommand(0, 0);
}
