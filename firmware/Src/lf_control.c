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
    pid->filtered_derivative = 0.0f;
    pid->prev_output = 0.0f;
    pid->initialized = 0U;
}

int16_t LF_Control_UpdatePid(float error, float dt_s, LF_PIDState *pid)
{
    float raw_derivative;
    float derivative;
    float output;
    float limited_output;
    float alpha = g_lf_config.derivative_filter_alpha;
    float integral_limit = g_lf_config.integral_limit;
    int16_t max_delta = g_lf_config.max_output_delta_per_tick;
    int16_t result;

    if (pid == 0 || dt_s <= 0.0f) {
        return 0;
    }

    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 0.98f) {
        alpha = 0.98f;
    }
    if (integral_limit < 0.0f) {
        integral_limit = -integral_limit;
    }

    if (pid->initialized == 0U) {
        pid->prev_error = error;
        pid->filtered_derivative = 0.0f;
        pid->prev_output = 0.0f;
        pid->initialized = 1U;
    }

    raw_derivative = (error - pid->prev_error) / dt_s;
    derivative = alpha * pid->filtered_derivative + (1.0f - alpha) * raw_derivative;
    pid->filtered_derivative = derivative;

    if (g_lf_config.ki != 0.0f) {
        pid->integral += error * dt_s;
        if (pid->integral > integral_limit) {
            pid->integral = integral_limit;
        } else if (pid->integral < -integral_limit) {
            pid->integral = -integral_limit;
        }
    }

    output = g_lf_config.kp * error + g_lf_config.ki * pid->integral + g_lf_config.kd * derivative;
    limited_output = (float)TDPS_ClampI16((int32_t)output,
                                          (int16_t)(-g_lf_config.max_correction),
                                          g_lf_config.max_correction);

    if (max_delta > 0) {
        float delta = limited_output - pid->prev_output;
        if (delta > (float)max_delta) {
            limited_output = pid->prev_output + (float)max_delta;
        } else if (delta < (float)(-max_delta)) {
            limited_output = pid->prev_output - (float)max_delta;
        }
    }

    result = TDPS_ClampI16((int32_t)limited_output,
                           (int16_t)(-g_lf_config.max_correction),
                           g_lf_config.max_correction);
    if ((result >= g_lf_config.max_correction && error > 0.0f) ||
        (result <= (int16_t)(-g_lf_config.max_correction) && error < 0.0f)) {
        pid->integral -= error * dt_s;
    }

    pid->prev_error = error;
    pid->prev_output = (float)result;
    return result;
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
