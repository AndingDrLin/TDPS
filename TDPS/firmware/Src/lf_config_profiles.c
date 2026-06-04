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
    g_lf_config.sensor_weights[0] =  1950;
    g_lf_config.sensor_weights[1] =  1450;
    g_lf_config.sensor_weights[2] =   950;
    g_lf_config.sensor_weights[3] =   250;
    g_lf_config.sensor_weights[4] =  -250;
    g_lf_config.sensor_weights[5] =  -750;
    g_lf_config.sensor_weights[6] = -1450;
    g_lf_config.sensor_weights[7] = -1950;
	
    g_lf_config.kp  = 0.25f;    // 提高P增益：S弯跟踪需要更强的比例响应
    g_lf_config.ki  = 0.05f;    // 小积分：消除弯道跟踪的稳态误差（P-only在曲线上是鞍点不稳定）
    g_lf_config.kd  = 0.60f;    // 倒三轮：提高D阻尼0.60→0.80，抑制高频振荡
		
    g_lf_config.control_error_deadband  = 30;    // 降低死区：S弯入口小偏差也能产生校正量，60→30
    g_lf_config.control_error_soft_zone = 120;   // 简化控制：不做软区衰减
		
    g_lf_config.max_correction            = 300;  // 提高差速上限，增强急弯转向能力
    g_lf_config.max_output_delta_per_tick = 20;   // 进一步降低单拍修正跳变(20→15)，倒三轮更平滑
    g_lf_config.max_motor_cmd             = 900;  // 恢复满功率，死区由 motor_deadband 处理
    g_lf_config.motor_deadband            = 120;
    g_lf_config.derivative_filter_alpha   = 0.35f; // D 项一阶低通，抑制 dt=0.01 下的噪声放大
    g_lf_config.integral_limit                = 500.0f;  // 积分上限：允许小积分消除弯道稳态误差
    g_lf_config.integral_separation_threshold = 400.0f;  // 积分分离：error>400 时积分衰减，防止弯道入口积分累积
    g_lf_config.integral_soft_zone            = 100.0f;  // 过渡区：400~500 线性降速积分

    g_lf_config.base_speed          = 100;   // 降速：给急弯控制器更多响应时间
    g_lf_config.min_speed           = 60;    // 弯道最低速度
    g_lf_config.kff                 = 0.0f;  // 倒三轮：传感器距轮轴仅 ~4cm，无有效预瞄距离，关前馈
    g_lf_config.steering_dir_sign   = -1;    // 倒三轮：电机方向翻转，删除此行会导致车反转

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
		
    g_lf_config.lead_compensation_enable           = false; // 倒三轮：传感器距轮轴仅 4cm，无物理前探距离，关前探
    g_lf_config.lead_event_active_count_threshold  = 6U;
    g_lf_config.lead_event_min_sum                 = 4000U;
    g_lf_config.lead_event_center_error_threshold  = 350;
    g_lf_config.lead_event_entry_error_threshold   = 650;
    g_lf_config.lead_event_confirm_ticks           = 2U;

    g_lf_config.lead_advance_ticks  = 18U;   // 直行 180ms，让车轮跟上传器位置
    g_lf_config.lead_advance_speed  = 65;     // 前探直行速度
    g_lf_config.lead_turn_hold_ticks = 16U;   // 前探后转向持续时间
    g_lf_config.lead_turn_speed     = 55;     // 前探转向基础速度
    g_lf_config.lead_turn_delta     = 115;    // 前探转向差速量
    g_lf_config.lead_entry_memory_ticks = 40U; // 入口方向记忆时长
		
    g_lf_config.line_stability_enable               = false;
    g_lf_config.stable_direction_enable             = true;
    g_lf_config.interference_active_count_threshold  = 5U;   /* 收紧：箭头处6+active应被检测为干扰 */
    g_lf_config.interference_position_jump_threshold = 2800;  /* 收紧：低速下280已是大跳变 */
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
    g_lf_config.start_straight_guard_enable = false;
    g_lf_config.start_straight_guard_ms = 1500U;
    g_lf_config.start_straight_guard_speed = 120;
    g_lf_config.start_straight_guard_max_correction = 15;
    g_lf_config.start_straight_guard_active_count = 4U;
    g_lf_config.start_straight_guard_release_ticks = 6U;
    g_lf_config.start_straight_guard_release_active_count = 3U;
    g_lf_config.start_straight_guard_release_error = 220;
    g_lf_config.calibration_duration_ms  = 500U;
    g_lf_config.recover_timeout_ms       = 3000U;
    g_lf_config.recover_turn_speed       = 120;
    g_lf_config.recover_backtrack_speed  = 80;
    g_lf_config.line_hold_speed          = 120;
    g_lf_config.line_hold_turn_speed     = 120;    /* 提高丢线时转向力度 */
    g_lf_config.edge_realign_enable      = false;
    g_lf_config.edge_realign_dir_sign    = 1;
    g_lf_config.edge_realign_speed       = 135;
    g_lf_config.edge_realign_delta       = 35;
    g_lf_config.edge_realign_confirm_ticks = 2U;
    g_lf_config.curve_arc_enable         = true;
    g_lf_config.curve_arc_speed          = 180;     // 弯道中低速，留更多修正余量
    g_lf_config.curve_arc_delta          = 140;    // 加大差速，减小转弯半径
    g_lf_config.curve_arc_max_motor_delta = 150;   // 匹配更大的 delta
    g_lf_config.curve_arc_confirm_ticks  = 2U;     // 2帧确认，过滤单帧噪声
    g_lf_config.curve_arc_release_ticks  = 4U;

    g_lf_config.reorient_enable           = true;   // 急弯停车+原地旋转对准
    g_lf_config.reorient_spin_speed       = 180;    // 原地旋转速度，提高到180加快对准
    g_lf_config.reorient_confirm_ticks    = 1U;     // 降低确认帧数，更快恢复巡线
    g_lf_config.reorient_timeout_ms       = 5000U;  // 增加超时，留足等待+旋转时间
    g_lf_config.reorient_position_threshold = 300;  // 降低门限700→300：覆盖单暗传感器场景(pos≈307)，配合C级检测+consistently_drifting防S弯误触发

    /* ===== 分段控制（debug profile 默认开启） ===== */
    g_lf_config.segment_control_enable    = true;

    g_lf_config.segment_confirm_ticks     = 1U;    /* 1帧确认，最快响应S弯入口（原2帧） */
    g_lf_config.segment_hold_ticks        = 3U;    /* 减少保持5→3：允许更快升级到TIGHT_CURVE */
    g_lf_config.segment_history_len       = 20U;

    /* 直道：稳速，小转向（与全局 base_speed=100 一致） */
    g_lf_config.seg_kp_straight           = 0.20f;  /* 提高0.15→0.20：直道也不过度抑制小偏差，S弯入口提前响应 */
    g_lf_config.seg_kd_straight           = 1.20f;
    g_lf_config.seg_kff_straight          = 0.0f;
    g_lf_config.seg_base_speed_straight   = 100;
    g_lf_config.seg_min_speed_straight    = 80;
    g_lf_config.seg_max_correction_straight = 250;

    /* 缓弯/S弯入口：快速响应，提高kp和max_correction */
    g_lf_config.seg_kp_gentle_curve       = 0.40f;  /* 提高0.35→0.40：更强初始弯道响应 */
    g_lf_config.seg_kd_gentle_curve       = 1.00f;  /* 适度D阻尼 */
    g_lf_config.seg_kff_gentle_curve      = 0.0008f;
    g_lf_config.seg_base_speed_gentle_curve = 90;    /* 降速100→90：S弯入口给更多反应时间 */
    g_lf_config.seg_min_speed_gentle_curve  = 75;     /* 降速80→75 */
    g_lf_config.seg_max_correction_gentle_curve = 350; /* 提高差速上限 */

    /* 急弯/连续弯：低速+大转向+低微分阻尼 */
    g_lf_config.seg_kp_tight_curve        = 0.45f;  /* 更高P增益，急弯全力转向 */
    g_lf_config.seg_kd_tight_curve        = 0.60f;  /* 降低D阻尼，减少转向抑制 */
    g_lf_config.seg_kff_tight_curve       = 0.0005f;
    g_lf_config.seg_base_speed_tight_curve = 80;    /* 继续降速 */
    g_lf_config.seg_min_speed_tight_curve  = 40;     /* 更低最低速 */
    g_lf_config.seg_max_correction_tight_curve = 500; /* 更大差速 */

    /* 宽线/路口/直角弯入口：极低速，保守转向 */
    g_lf_config.seg_kp_wide_line          = 0.20f;
    g_lf_config.seg_kd_wide_line          = 1.00f;
    g_lf_config.seg_kff_wide_line         = 0.0f;
    g_lf_config.seg_base_speed_wide_line  = 80;
    g_lf_config.seg_min_speed_wide_line   = 50;
    g_lf_config.seg_max_correction_wide_line = 250;

    /* 岔路口：最低速，保守转向 */
    g_lf_config.seg_kp_fork               = 0.20f;
    g_lf_config.seg_kd_fork               = 1.00f;
    g_lf_config.seg_kff_fork              = 0.0f;
    g_lf_config.seg_base_speed_fork       = 60;
    g_lf_config.seg_min_speed_fork        = 50;
    g_lf_config.seg_max_correction_fork   = 200;

    /* 连续弯 */
    g_lf_config.seg_curve_direction_window    = 20U;
    g_lf_config.seg_curve_direction_switch_min = 2U;
    g_lf_config.seg_curve_grace_ticks_extra   = 2U;  /* 减少grace，让紧急转向更早 */

    /* 直角转弯增强 */
    g_lf_config.right_angle_confirm_ticks  = 2U;
    g_lf_config.reorient_approach_speed    = 80;
    g_lf_config.reorient_approach_ms       = 0U;    /* 不前进靠近弯点，直接停车等待 */
    g_lf_config.reorient_stop_ms           = 1000U; /* 停车等待1秒，车身完全静止后再旋转 */
    g_lf_config.reorient_backtrack_enable  = true;
    g_lf_config.reorient_backtrack_speed   = 120;
    g_lf_config.reorient_backtrack_ms      = 400U;
    g_lf_config.reorient_max_retries       = 2U;
    g_lf_config.reorient_cooldown_ms       = 1500U;  /* reorient完成后1.5秒冷却，防U弯振荡 */

    g_lf_config.radar_enable         = false;
    g_lf_config.obstacle_avoid_enable = false;
    g_lf_config.fork_enable          = false;
}

void LF_Config_ApplyCompetitionProfile(void)
{
    LF_Config_ApplyDebugProfile();

    g_lf_config.base_speed = 300;
    g_lf_config.adaptive_slow_speed = 200;
    g_lf_config.max_correction = 400;
    g_lf_config.max_motor_cmd = 900;
    g_lf_config.auto_start_delay_ms = 800U;
    g_lf_config.sensor_fast_calibration = false;
    g_lf_config.fork_enable = true;
    g_lf_config.recover_timeout_ms = 1100U;
}
