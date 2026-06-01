#include "lf_config.h"

/*
 * 配置预设：修改本文件中的 profile 函数来切换测试 / 比赛参数。
 * 在 main.c 中调用一个 profile 函数即可覆盖默认值。
 */

void LF_Config_ApplyDebugProfile(void)
{
    g_lf_config.sensor_input_mode              = LF_SENSOR_INPUT_UART_PROTOCOL;
    g_lf_config.sensor_use_dynamic_calibration = false;
    g_lf_config.sensor_fast_calibration        = true;
    g_lf_config.sensor_invert_polarity         = true;
    g_lf_config.sensor_digital_active_high     = false;
    g_lf_config.sensor_filter_alpha            = 0.35f;
    g_lf_config.line_detect_min_sum            = 780U;
    g_lf_config.line_detect_min_peak           = 260U;
    g_lf_config.line_detect_min_contrast       = 80U;
    g_lf_config.sensor_edge_noise_reject_enable    = true;
    g_lf_config.sensor_edge_noise_neighbor_threshold = 220U;
    g_lf_config.sensor_weights[0] =  1750;
    g_lf_config.sensor_weights[1] =  1250;
    g_lf_config.sensor_weights[2] =   750;
    g_lf_config.sensor_weights[3] =   250;
    g_lf_config.sensor_weights[4] =  -250;
    g_lf_config.sensor_weights[5] =  -750;
    g_lf_config.sensor_weights[6] = -1250;
    g_lf_config.sensor_weights[7] = -1750;
	
    g_lf_config.kp  = 0.22f;    // 旧验证值：直线上有死区保护，P 项可以给足
    g_lf_config.ki  = 0.0f;     // 积分：不开（会累积画龙）
    g_lf_config.kd  = 0.35f;    // 旧验证值：配合 alpha=0.35 微分滤波，避免 dt=0.01 噪声放大
		
    g_lf_config.control_error_deadband  = 15;    // 死区：吸收 ±15 以内的 sensor 噪声
    g_lf_config.control_error_soft_zone = 0;    // 无软区——线性响应，不用二次曲线
		
    g_lf_config.max_correction            = 300;  // 差速硬上限
    g_lf_config.max_output_delta_per_tick = 18;   // 旧验证值：单拍修正最多变 18，渐进转向
    g_lf_config.max_motor_cmd             = 300;
    g_lf_config.motor_deadband            = 0;
    g_lf_config.derivative_filter_alpha   = 0.35f; // D 项一阶低通，抑制 dt=0.01 下的噪声放大
    g_lf_config.integral_limit                = 0.0f;  // 无积分——巡线无静差
    g_lf_config.integral_separation_threshold = 0.0f;
    g_lf_config.integral_soft_zone            = 0.0f;

    g_lf_config.base_speed          = 300;   // 旧验证值：有死区+滤波+限幅后直线可跑全速
    g_lf_config.min_speed           = 60;    // 弯道最低速度
    g_lf_config.kff                 = 0.0f;  // 先关 kff 测纯 PD：直线稳后再从小到大加

		g_lf_config.adaptive_slow_speed       = 60;    // 低置信度/低对比度时的速度
    g_lf_config.adaptive_error_threshold  = 100;    // ★ 位置超阈值立即触发 sharp 降速（第一优先级）
    g_lf_config.adaptive_confidence_threshold = 0.40f;  // 默认值
    g_lf_config.sharp_turn_speed          = 60;    // ★ 最慢爬行速度，尖角极限转向

    g_lf_config.straight_boost_enable          = false;
    g_lf_config.curve_prepare_enable           = false; // 连续速度函数替代
    g_lf_config.curve_prepare_error_threshold  = 80;    // ★ 位置超阈值开始计弯道帧
    g_lf_config.curve_prepare_delta_threshold  = 100;    // ★ 位置跳变超阈值也计入
    g_lf_config.curve_prepare_confirm_ticks    = 3U;    // ★ 几帧之后确认进入弯道
    g_lf_config.curve_prepare_speed            = 30;    // ★ 弯道速度
		
    g_lf_config.lead_compensation_enable           = false;
    g_lf_config.lead_event_active_count_threshold  = 6U;
    g_lf_config.lead_event_min_sum                 = 2200U;
    g_lf_config.lead_event_center_error_threshold  = 350;
    g_lf_config.lead_event_entry_error_threshold   = 650;
    g_lf_config.lead_event_confirm_ticks           = 2U;

    g_lf_config.lead_advance_ticks  = 18U;   // 直行 180ms，让车轮跟上传器位置
    g_lf_config.lead_advance_speed  = 65;     // 前探直行速度
    g_lf_config.lead_turn_hold_ticks = 16U;   // 前探后转向持续时间
    g_lf_config.lead_turn_speed     = 55;     // 前探转向基础速度
    g_lf_config.lead_turn_delta     = 115;    // 前探转向差速量
    g_lf_config.lead_entry_memory_ticks = 40U; // 入口方向记忆时长
		
    g_lf_config.line_stability_enable               = true;
    g_lf_config.stable_direction_enable             = true;
    g_lf_config.interference_active_count_threshold  = 5U;
    g_lf_config.interference_position_jump_threshold = 200;
    g_lf_config.direction_update_confidence_min      = 0.50f;

    g_lf_config.straight_noise_reject_enable         = true;
    g_lf_config.straight_noise_confirm_ticks         = 2U;
    g_lf_config.straight_noise_active_count_threshold = 5U;
    g_lf_config.straight_noise_max_sum               = 1800U;
    g_lf_config.straight_noise_max_position_error    = 250;
    g_lf_config.straight_noise_max_position_delta    = 120;
		
    g_lf_config.auto_start_delay_ms      = 0U;
    g_lf_config.start_min_boot_delay_ms  = 2000U;
    g_lf_config.start_line_hold_ms       = 500U;
    g_lf_config.calibration_duration_ms  = 500U;
    g_lf_config.recover_timeout_ms       = 3000U;
    g_lf_config.recover_turn_speed       = 0;
    g_lf_config.recover_backtrack_speed  = 0;
    g_lf_config.line_hold_speed          = 0;
    g_lf_config.line_hold_turn_speed     = 0;
		
    g_lf_config.radar_enable         = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable          = false;
}

void LF_Config_ApplyCompetitionProfile(void)
{
    LF_Config_ApplyDebugProfile();

    g_lf_config.base_speed = 400;
    g_lf_config.adaptive_slow_speed = 280;
    g_lf_config.max_correction = 400;
    g_lf_config.max_motor_cmd = 900;
    g_lf_config.auto_start_delay_ms = 800U;
    g_lf_config.sensor_fast_calibration = false;
    g_lf_config.fork_enable = true;
    g_lf_config.recover_timeout_ms = 1100U;
}
