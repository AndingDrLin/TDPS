/**
 * @file lf_control.c
 * @brief Line-following PD + curvature-feedforward controller.
 *
 * Core formula:
 *   correction = kp * error + kd * derivative + kff * derivative * speed
 *
 * Uses backward-Euler differentiation with a first-order low-pass filter
 * on the derivative term to suppress sensor noise amplification.
 */
#include "lf_control.h"

#include "clamp.h"
#include "lf_config.h"

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

/**
 * @brief Core PD + kff computation shared by both UpdatePD variants.
 *
 * @param error          Position error (positive = line to the right).
 * @param dt_s           Time delta in seconds.
 * @param speed          Current motor speed (for feedforward scaling).
 * @param pid            PID state to read/write.
 * @param kp             Proportional gain.
 * @param kd             Derivative gain.
 * @param kff            Curvature feedforward gain.
 * @param max_correction Maximum absolute correction output.
 * @return Clamped correction value in [-max_correction, +max_correction].
 */
static int16_t update_pd_core(float error,
                              float dt_s,
                              int16_t speed,
                              LF_PIDState *pid,
                              float kp,
                              float kd,
                              float kff,
                              int16_t max_correction)
{
    float alpha;
    float raw_derivative;
    float derivative;
    float output;
    int16_t max_delta;

    if (pid == 0 || dt_s <= 0.0f || max_correction <= 0) {
        return 0;
    }
    if (dt_s < 0.001f) { dt_s = 0.001f; }

    alpha = g_lf_config.derivative_filter_alpha;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 0.98f) {
        alpha = 0.98f;
    }

    if (pid->initialized == 0U) {
        pid->prev_error = error;
        pid->filtered_derivative = 0.0f;
        pid->prev_output = 0.0f;
        pid->initialized = 1U;
    }

    /* Backward-Euler derivative + first-order low-pass:
     * suppresses sensor noise from being amplified by the D term. */
    raw_derivative = (error - pid->prev_error) / dt_s;
    derivative = alpha * pid->filtered_derivative + (1.0f - alpha) * raw_derivative;
    pid->filtered_derivative = derivative;

    output = kp * error + kd * derivative;
    if (kff != 0.0f && speed > 0) {
        output += kff * derivative * (float)speed;
    }

    output = (float)TDPS_ClampI16((int32_t)output,
                                  (int16_t)(-max_correction),
                                  max_correction);

    /* Per-tick output rate limiter: prevents a single bad sensor frame
     * from causing an abrupt steering correction. */
    max_delta = g_lf_config.max_output_delta_per_tick;
    if (max_delta > 0) {
        float delta = output - pid->prev_output;
        if (delta > (float)max_delta) {
            output = pid->prev_output + (float)max_delta;
        } else if (delta < (float)(-max_delta)) {
            output = pid->prev_output - (float)max_delta;
        }
    }

    pid->prev_error = error;
    pid->prev_output = output;
    return TDPS_ClampI16((int32_t)output,
                         (int16_t)(-max_correction),
                         max_correction);
}

int16_t LF_Control_UpdatePD(float error, float dt_s, int16_t speed, LF_PIDState *pid)
{
    return update_pd_core(error,
                          dt_s,
                          speed,
                          pid,
                          g_lf_config.kp,
                          g_lf_config.kd,
                          g_lf_config.kff,
                          g_lf_config.max_correction);
}

int16_t LF_Control_UpdatePDWithParams(float error,
                                      float dt_s,
                                      int16_t speed,
                                      LF_PIDState *pid,
                                      float kp,
                                      float kd,
                                      float kff,
                                      int16_t max_correction)
{
    return update_pd_core(error, dt_s, speed, pid, kp, kd, kff, max_correction);
}

void LF_Control_ComputeMotorCmd(int16_t base_speed,
                                int16_t correction,
                                int16_t *out_left,
                                int16_t *out_right)
{
    int8_t steering_sign = (g_lf_config.steering_dir_sign < 0) ? -1 : +1;
    int32_t left;
    int32_t right;

    correction = (int16_t)((int16_t)steering_sign * correction);
    left = (int32_t)base_speed + correction;
    right = (int32_t)base_speed - correction;

    if (out_left != 0) {
        *out_left = TDPS_ClampI16(left,
                                  (int16_t)(-g_lf_config.max_motor_cmd),
                                  g_lf_config.max_motor_cmd);
    }
    if (out_right != 0) {
        *out_right = TDPS_ClampI16(right,
                                   (int16_t)(-g_lf_config.max_motor_cmd),
                                   g_lf_config.max_motor_cmd);
    }
}
