/**
 * @file lf_control.h
 * @brief 巡线 PD + 曲率前馈控制器
 */
#ifndef LF_CONTROL_H
#define LF_CONTROL_H

#include <stdint.h>

/**
 * @brief 控制器运行状态
 *
 * 仅保留 PD 所需历史量。integral 字段保留为兼容调试观察，
 * 新控制路径不再使用积分，避免弯道积分记忆导致出弯过冲。
 */
typedef struct {
    float integral;
    float prev_error;
    float filtered_derivative;
    float prev_output;
    uint8_t initialized;
} LF_PIDState;

/** 重置控制器状态 */
void LF_Control_ResetPid(LF_PIDState *pid);

/**
 * @brief PD + kff 更新
 *
 * correction = kp*error + kd*derivative + kff*derivative*speed
 */
int16_t LF_Control_UpdatePD(float error, float dt_s, int16_t speed, LF_PIDState *pid);

/** 与 LF_Control_UpdatePD 相同，但使用传入参数，供分段控制平滑切换参数集 */
int16_t LF_Control_UpdatePDWithParams(float error,
                                      float dt_s,
                                      int16_t speed,
                                      LF_PIDState *pid,
                                      float kp,
                                      float kd,
                                      float kff,
                                      int16_t max_correction);

/** 将基础速度和修正量合成为左右轮命令 */
void LF_Control_ComputeMotorCmd(int16_t base_speed,
                                int16_t correction,
                                int16_t *out_left,
                                int16_t *out_right);

#endif /* LF_CONTROL_H */
