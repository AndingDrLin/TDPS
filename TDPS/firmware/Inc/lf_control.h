#ifndef LF_CONTROL_H
#define LF_CONTROL_H

#include <stdint.h>

typedef struct {
    float integral;
    float prev_error;
    float filtered_derivative;
    float prev_output;
    uint8_t initialized;
} LF_PIDState;

/* PID 状态初始化。 */
void LF_Control_ResetPid(LF_PIDState *pid);

/* 基于当前偏差计算修正量。输出单位与电机命令一致（-1000~1000）。 */
int16_t LF_Control_UpdatePid(float error, float dt_s, LF_PIDState *pid);

/* 将基础速度和修正量合成为左右轮命令。 */
void LF_Control_ComputeMotorCmd(int16_t base_speed, int16_t correction, int16_t *out_left, int16_t *out_right);

/* 简化 PD+kff（无积分、无变化率限幅、无反饱和、无微分二次滤波）。
 * speed 用于 kff 前馈项: kff * derivative * speed。 */
int16_t LF_Control_UpdatePD(float error, float dt_s, int16_t speed, LF_PIDState *pid);

#endif /* LF_CONTROL_H */
