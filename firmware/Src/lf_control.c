#include "lf_control.h"

#include "lf_config.h"
#include "clamp.h"

void LF_Control_ResetPid(LF_PIDState *pid)
{
    if (pid == 0) {
        return;
    }
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->initialized = 0U;
}

int16_t LF_Control_UpdatePid(float error, float dt_s, LF_PIDState *pid)
{
    float derivative;
    float output;

    if (pid == 0 || dt_s <= 0.0f) {
        return 0;
    }

    if (pid->initialized == 0U) {
        pid->prev_error = error;
        pid->initialized = 1U;
    }

    if (g_lf_config.ki != 0.0f) {
        pid->integral += error * dt_s;

        /* 简单积分限幅，防止长期偏差积累导致过冲。 */
        if (pid->integral > 5000.0f) {
            pid->integral = 5000.0f;
        } else if (pid->integral < -5000.0f) {
            pid->integral = -5000.0f;
        }
    }

    derivative = (error - pid->prev_error) / dt_s;
    output = g_lf_config.kp * error + g_lf_config.ki * pid->integral + g_lf_config.kd * derivative;
    pid->prev_error = error;

    return TDPS_ClampI16((int32_t)output, (int16_t)(-g_lf_config.max_correction), g_lf_config.max_correction);
}

void LF_Control_ComputeMotorCmd(int16_t base_speed, int16_t correction, int16_t *out_left, int16_t *out_right)
{
    int32_t left = (int32_t)base_speed - correction;
    int32_t right = (int32_t)base_speed + correction;

    if (out_left != 0) {
        *out_left = TDPS_ClampI16(left, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
    if (out_right != 0) {
        *out_right = TDPS_ClampI16(right, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
}
