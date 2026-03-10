#include "lf_control.h"

#include "lf_config.h"

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

    pid->integral += error * dt_s;

    /* 简单积分限幅，防止长期偏差积累导致过冲。 */
    if (pid->integral > 5000.0f) {
        pid->integral = 5000.0f;
    } else if (pid->integral < -5000.0f) {
        pid->integral = -5000.0f;
    }

    derivative = (error - pid->prev_error) / dt_s;
    output = g_lf_config.kp * error + g_lf_config.ki * pid->integral + g_lf_config.kd * derivative;
    pid->prev_error = error;

    return clamp_i16((int32_t)output, (int16_t)(-g_lf_config.max_correction), g_lf_config.max_correction);
}

void LF_Control_ComputeMotorCmd(int16_t base_speed, int16_t correction, int16_t *out_left, int16_t *out_right)
{
    int32_t left = (int32_t)base_speed - correction;
    int32_t right = (int32_t)base_speed + correction;

    if (out_left != 0) {
        *out_left = clamp_i16(left, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
    if (out_right != 0) {
        *out_right = clamp_i16(right, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
}
