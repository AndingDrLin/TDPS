/**
 * @file lf_control.h
 * @brief Line-following PD + curvature feedforward controller.
 */
#ifndef LF_CONTROL_H
#define LF_CONTROL_H

#include <stdint.h>

/**
 * @brief Controller runtime state.
 *
 * Only the history quantities required by PD are kept. The integral field
 * is retained for debugging compatibility; the new control path no longer
 * uses integration to avoid integral windup causing overshoot on curve exit.
 */
typedef struct {
    float integral;
    float prev_error;
    float filtered_derivative;
    float prev_output;
    uint8_t initialized;
} LF_PIDState;

/** @brief Reset the controller state.
 *  @param[out] pid Pointer to the PID state to reset.
 */
void LF_Control_ResetPid(LF_PIDState *pid);

/**
 * @brief PD + kff update.
 *
 * correction = kp*error + kd*derivative + kff*derivative*speed
 *
 * @param error   Position error from the sensor.
 * @param dt_s    Time delta in seconds.
 * @param speed   Current base speed.
 * @param[in,out] pid Pointer to the PID state.
 * @return Motor correction value.
 */
int16_t LF_Control_UpdatePD(float error, float dt_s, int16_t speed, LF_PIDState *pid);

/**
 * @brief Same as LF_Control_UpdatePD, but uses caller-supplied parameters.
 *
 * Used for per-segment smooth parameter set switching.
 *
 * @param error          Position error from the sensor.
 * @param dt_s           Time delta in seconds.
 * @param speed          Current base speed.
 * @param[in,out] pid    Pointer to the PID state.
 * @param kp             Proportional gain.
 * @param kd             Derivative gain.
 * @param kff            Curvature feedforward gain.
 * @param max_correction Output clamp.
 * @return Motor correction value.
 */
int16_t LF_Control_UpdatePDWithParams(float error,
                                      float dt_s,
                                      int16_t speed,
                                      LF_PIDState *pid,
                                      float kp,
                                      float kd,
                                      float kff,
                                      int16_t max_correction);

/**
 * @brief Combine base speed and correction into left/right wheel commands.
 *
 * @param      base_speed Base forward speed.
 * @param      correction PD correction value.
 * @param[out] out_left   Pointer to the left wheel output command.
 * @param[out] out_right  Pointer to the right wheel output command.
 */
void LF_Control_ComputeMotorCmd(int16_t base_speed,
                                int16_t correction,
                                int16_t *out_left,
                                int16_t *out_right);

#endif /* LF_CONTROL_H */
