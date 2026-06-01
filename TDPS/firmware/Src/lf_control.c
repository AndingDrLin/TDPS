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

/*
 * 位置式 PID 更新（100Hz 控制周期）。
 *
 * 误差约定：position 左负右正，目标值 0。
 * error > 0 → 需向右修正（左轮加速、右轮减速）。
 *
 * 保护链：积分分离 → 硬限幅 → 输出限幅 → 变化率限幅 → 反饱和回退。
 * D 项经一阶低通滤波抑制高频噪声，alpha 越大 D 越平滑（响应越慢）。
 */
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

    /* 状态切换后首拍：用当前误差初始化，防止微分突变 */
    if (pid->initialized == 0U) {
        pid->prev_error = error;
        pid->filtered_derivative = 0.0f;
        pid->prev_output = 0.0f;
        pid->initialized = 1U;
    }

    /* 后向欧拉微分 + 一阶低通滤波 */
    raw_derivative = (error - pid->prev_error) / dt_s;
    derivative = alpha * pid->filtered_derivative + (1.0f - alpha) * raw_derivative;
    pid->filtered_derivative = derivative;

    if (g_lf_config.ki != 0.0f) {
        float abs_error = (error > 0.0f) ? error : -error;
        float sep = g_lf_config.integral_separation_threshold;
        float soft = g_lf_config.integral_soft_zone;

        if (sep > 0.0f && abs_error > sep + soft) {
            /*
             * 大误差区（如弯道）：积分指数衰减，防止弯道积分记忆导致出弯过冲。
             * 衰减系数 0.90 每拍，10 拍（100ms）内降至原来的 35%。
             */
            pid->integral *= 0.90f;
        } else if (sep > 0.0f && abs_error > sep) {
            /*
             * 过渡区：积分速率从 100% 线性降至 0%。
             * 误差越接近分离阈值，积分越慢。
             */
            float ratio = (abs_error - sep) / soft;
            float speed = 1.0f - ratio;
            pid->integral += error * dt_s * speed;
        } else {
            /*
             * 小误差区：正常全速积分。
             * 这是直线上的稳态修正窗口。
             */
            pid->integral += error * dt_s;
        }

        /* 硬限幅 */
        if (pid->integral > integral_limit) {
            pid->integral = integral_limit;
        } else if (pid->integral < -integral_limit) {
            pid->integral = -integral_limit;
        }
    }

    /* P + I + D 合成，输出限幅到 max_correction */
    output = g_lf_config.kp * error + g_lf_config.ki * pid->integral + g_lf_config.kd * derivative;
    limited_output = (float)TDPS_ClampI16((int32_t)output,
                                          (int16_t)(-g_lf_config.max_correction),
                                          g_lf_config.max_correction);

    /* 输出变化率限幅：单拍最大跳变 max_output_delta_per_tick，防止传感器跳变导致急转 */
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

    /* 条件式反饱和：输出已触顶且误差仍往同方向推 → 撤销本拍积分增量 */
    if ((result >= g_lf_config.max_correction && error > 0.0f) ||
        (result <= (int16_t)(-g_lf_config.max_correction) && error < 0.0f)) {
        pid->integral -= error * dt_s;
    }

    pid->prev_error = error;
    pid->prev_output = (float)result;
    return result;
}

/*
 * 简化 PD+kff 控制（无积分、无反饱和回退）。
 * 用于 TDPS_SIMPLE_CONTROL 模式，配合连续速度函数。
 *
 * correction = kp*error + kd*derivative + kff*derivative*speed
 * kff 项利用传感器 22cm 预瞄优势——不等偏差出现就预打方向。
	 *
	 * 保留两层实车必需的保护：
	 *   derivative_filter_alpha  — D 项一阶低通滤波，抑制传感器噪声放大
	 *   max_output_delta_per_tick — 单拍修正变化上限，防止 overshoot 正反馈
 */
int16_t LF_Control_UpdatePD(float error, float dt_s, int16_t speed, LF_PIDState *pid)
{
    float raw_derivative;
    float derivative;
    float output;
    float alpha = g_lf_config.derivative_filter_alpha;
    int16_t max_delta = g_lf_config.max_output_delta_per_tick;

    if (pid == 0 || dt_s <= 0.0f) {
        return 0;
    }

    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 0.98f) alpha = 0.98f;

    if (pid->initialized == 0U) {
        pid->prev_error = error;
        pid->filtered_derivative = 0.0f;
        pid->prev_output = 0.0f;
        pid->initialized = 1U;
    }

    raw_derivative = (error - pid->prev_error) / dt_s;
    derivative = alpha * pid->filtered_derivative + (1.0f - alpha) * raw_derivative;
    pid->filtered_derivative = derivative;

    output = g_lf_config.kp * error + g_lf_config.kd * derivative;

    if (g_lf_config.kff != 0.0f && speed > 0) {
        output += g_lf_config.kff * derivative * (float)speed;
    }

    if (max_delta > 0) {
        float delta_output = output - pid->prev_output;
        if (delta_output > (float)max_delta) {
            output = pid->prev_output + (float)max_delta;
        } else if (delta_output < (float)(-max_delta)) {
            output = pid->prev_output - (float)max_delta;
        }
    }

    pid->prev_error = error;
    pid->prev_output = output;
    return (int16_t)TDPS_ClampI16((int32_t)output,
                                   (int16_t)(-g_lf_config.max_correction),
                                   g_lf_config.max_correction);
}

void LF_Control_ComputeMotorCmd(int16_t base_speed, int16_t correction, int16_t *out_left, int16_t *out_right)
{
    int32_t left = (int32_t)base_speed + correction;
    int32_t right = (int32_t)base_speed - correction;

    if (out_left != 0) {
        *out_left = TDPS_ClampI16(left, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
    if (out_right != 0) {
        *out_right = TDPS_ClampI16(right, (int16_t)(-g_lf_config.max_motor_cmd), g_lf_config.max_motor_cmd);
    }
}
