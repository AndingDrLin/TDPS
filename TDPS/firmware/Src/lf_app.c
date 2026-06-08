/**
 * @file lf_app.c
 * @brief Line-following application state machine (main orchestrator).
 *
 * Design principles:
 * 1. The main loop never blocks; all actions advance through a state machine
 *    driven by timestamps.
 * 2. Normal line following uses a single PD + curvature-feedforward controller.
 * 3. Complex intersections are handled through a "route script + fixed 90-degree
 *    turn" scheme, not scattered ad-hoc checks.
 * 4. Right-angle detection has a single entry point to avoid conflicting rules.
 *
 * Sub-modules:
 * - lf_app_util.c    : timing helpers, state transitions, speed limiting
 * - lf_app_sensor.c  : lane queries, right-angle detection, line confirmation
 * - lf_app_route.c   : fixed-route script for the competition course
 * - lf_app_segment.c : segment classification for adaptive PD parameters
 * - lf_app_avoid.c   : obstacle avoidance, fixed turns, reorientation
 */
#include "lf_app.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "lf_app_avoid.h"
#include "lf_app_internal.h"
#include "lf_app_route.h"
#include "lf_app_segment.h"
#include "lf_app_sensor.h"
#include "lf_app_util.h"
#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_future_hooks.h"
#include "lf_led_blink.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"
#include "lf_sensor_uart.h"
#include "lf_ultrasonic.h"
#include "lf_watch_debug.h"

/* ==================== Global State ==================== */

/** Main application context, shared with sub-modules via lf_app_internal.h. */
LF_AppContext g_app;

/** Digital sensor cache for D-frame data (Yahboom UART protocol). */
static uint8_t s_digital_cache[LF_SENSOR_COUNT];

/** Whether s_digital_cache contains valid data from the latest frame. */
static bool s_digital_cache_valid;

/** Pending route phase awaiting commit (0xFF = no pending). */
static uint8_t s_route_pending_phase = 0xFFU;

/* ==================== Shared Accessors ==================== */

uint8_t *LF_App_GetDigitalCache(void)
{
    return s_digital_cache;
}

bool LF_App_IsDigitalCacheValid(void)
{
    return s_digital_cache_valid;
}

void LF_App_SetDigitalCacheValid(bool valid)
{
    s_digital_cache_valid = valid;
}

uint8_t *LF_App_GetRoutePendingPhase(void)
{
    return &s_route_pending_phase;
}

/* ==================== Trusted Direction Tracking ==================== */

/**
 * @brief Update the trusted line direction from the latest sensor frame.
 *
 * Tracks the last known line position and direction so that line-loss
 * recovery can sweep in the correct direction.
 */
static void update_trusted_direction(void)
{
    int8_t dir = 0;

    if (!g_app.last_frame.line_detected) {
        return;
    }

    if (g_app.last_frame.edge_hint != 0) {
        dir = g_app.last_frame.edge_hint;
    } else if (g_app.last_frame.position < 0) {
        dir = -1;
    } else if (g_app.last_frame.position > 0) {
        dir = +1;
    }

    if (dir != 0) {
        g_app.last_seen_dir = dir;
        g_app.trusted_line_dir = dir;
        g_app.last_trusted_position = g_app.last_frame.position;
        g_app.trusted_line_valid = true;
    }
}

/**
 * @brief Drive the car forward while turning toward the last known line direction.
 *
 * Used during brief line-loss grace ticks before full recovery starts.
 */
static void hold_last_line_direction(void)
{
    int16_t forward = lf_limit_degraded_speed((int16_t)(g_lf_config.min_speed));
    int16_t turn = lf_limit_degraded_speed(g_lf_config.recover_sweep_speed);
    int8_t dir = g_app.trusted_line_valid ? g_app.trusted_line_dir : g_app.last_seen_dir;

    if (dir < 0) {
        LF_Chassis_SetCommand((int16_t)(forward - turn), (int16_t)(forward + turn));
    } else {
        LF_Chassis_SetCommand((int16_t)(forward + turn), (int16_t)(forward - turn));
    }
}

/* ==================== Line-Loss Recovery ==================== */

/**
 * @brief Enter the recovery state machine after losing the line.
 * @param now_ms Current timestamp.
 */
static void start_recovery(uint32_t now_ms)
{
    g_app.recover_start_ms = now_ms;
    g_app.recover_phase_start_ms = now_ms;
    g_app.recover_phase = (uint8_t)LF_RECOVER_FORWARD;
    g_app.recover_confirm_count = 0U;
    lf_set_state(LF_APP_STATE_RECOVERING);
}

/**
 * @brief Process the recovery state machine.
 *
 * Recovery phases:
 * - FORWARD: drive forward briefly to try re-acquiring the line.
 * - BACKTRACK: reverse to the position where the line was last seen.
 * - SWEEP_LEFT / SWEEP_RIGHT: rotate in alternating directions.
 * - At any point, if the line is re-acquired or a fixed turn is detected,
 *   resume normal operation.
 *
 * @param now_ms Current timestamp.
 */
static void process_recovery(uint32_t now_ms)
{
    uint32_t phase_elapsed = now_ms - g_app.recover_phase_start_ms;
    int16_t speed;

    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();

    /* During recovery, still check for fixed-turn opportunities. */
    if (LF_App_TryStartFixedTurn(now_ms)) {
        return;
    }

    /* Line re-acquired: resume running. */
    if (LF_App_LineConfirmed(&g_app.recover_confirm_count, 3U)) {
        g_app.recovery_count++;
        LF_Control_ResetPid(&g_app.pid);
        g_app.line_lost_count = 0U;
        g_app.segment_type = LF_SEGMENT_STRAIGHT;
        lf_set_state(LF_APP_STATE_RUNNING);
        return;
    }

    /* Maximum recovery count exceeded: stop the car to prevent
     * infinite recovery loops. */
    if (g_app.recovery_count >= 100U) {
        LF_Chassis_Stop();
        lf_set_state(LF_APP_STATE_STOPPED);
        return;
    }

    /* Overall recovery timeout: stop the car. */
    if (lf_time_reached(now_ms, g_app.recover_start_ms, g_lf_config.recover_timeout_ms)) {
        LF_Chassis_Stop();
        lf_set_state(LF_APP_STATE_STOPPED);
        return;
    }

    switch ((LF_RecoverPhase)g_app.recover_phase) {
    case LF_RECOVER_FORWARD:
        speed = lf_limit_degraded_speed(g_lf_config.recover_forward_speed);
        LF_Chassis_SetCommand(speed, speed);
        if (phase_elapsed >= g_lf_config.recover_forward_ms) {
            g_app.recover_phase = (uint8_t)LF_RECOVER_BACKTRACK;
            g_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_BACKTRACK:
        speed = lf_limit_degraded_speed(g_lf_config.recover_backtrack_speed);
        LF_Chassis_SetCommand((int16_t)(-speed), (int16_t)(-speed));
        if (phase_elapsed >= g_lf_config.recover_backtrack_ms) {
            /* Sweep in the direction where the line was last seen. */
            g_app.recover_phase = (g_app.last_seen_dir < 0) ?
                (uint8_t)LF_RECOVER_SWEEP_LEFT : (uint8_t)LF_RECOVER_SWEEP_RIGHT;
            g_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_SWEEP_LEFT:
        speed = lf_limit_degraded_speed(g_lf_config.recover_sweep_speed);
        LF_Chassis_SetCommand((int16_t)(-speed), speed);
        if (phase_elapsed >= g_lf_config.recover_sweep_ms) {
            g_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_RIGHT;
            g_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_SWEEP_RIGHT:
        speed = lf_limit_degraded_speed(g_lf_config.recover_sweep_speed);
        LF_Chassis_SetCommand(speed, (int16_t)(-speed));
        if (phase_elapsed >= g_lf_config.recover_sweep_ms) {
            g_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_LEFT;
            g_app.recover_phase_start_ms = now_ms;
        }
        break;

    case LF_RECOVER_CONFIRM:
    default:
        LF_Chassis_Stop();
        break;
    }
}

/* ==================== PD Control ==================== */

/**
 * @brief Run one frame of PD + curvature-feedforward line-following control.
 *
 * Selects segment-adaptive parameters (straight vs. curve) when segment
 * control is enabled, applies the speed profile with IIR smoothing,
 * computes the correction, and drives the chassis.
 *
 * @param dt_s Time delta since the last control frame, in seconds.
 */
static void run_pd_control(float dt_s)
{
    float error;
    float raw_abs;
    float ratio;
    int16_t raw_speed;
    int16_t base_speed;
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    float kp = g_lf_config.kp;
    float kd = g_lf_config.kd;
    float kff = g_lf_config.kff;
    int16_t target_base = g_lf_config.base_speed;
    int16_t target_min = g_lf_config.min_speed;
    int16_t target_max_corr = g_lf_config.max_correction;

    /* Select segment-adaptive parameters when enabled. */
    if (g_lf_config.segment_control_enable && g_app.segment_type != LF_SEGMENT_STRAIGHT) {
        kp = g_lf_config.seg_kp_curve;
        kd = g_lf_config.seg_kd_curve;
        kff = g_lf_config.seg_kff_curve;
        target_base = g_lf_config.seg_base_speed_curve;
        target_min = g_lf_config.seg_min_speed_curve;
        target_max_corr = g_lf_config.seg_max_correction_curve;
    } else if (g_lf_config.segment_control_enable) {
        kp = g_lf_config.seg_kp_straight;
        kd = g_lf_config.seg_kd_straight;
        kff = g_lf_config.seg_kff_straight;
        target_base = g_lf_config.seg_base_speed_straight;
        target_min = g_lf_config.seg_min_speed_straight;
        target_max_corr = g_lf_config.seg_max_correction_straight;
    }

    /* Speed profile: linearly reduce speed as error grows. */
    error = (float)g_app.last_frame.position;
    raw_abs = (error < 0.0f) ? -error : error;
    ratio = raw_abs / 1750.0f;
    if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    raw_speed = (int16_t)((float)target_base -
                          (float)(target_base - target_min) * ratio);
    if (raw_speed < target_min) {
        raw_speed = target_min;
    }

    /* First-order IIR smoothing to avoid speed jumps at curve entry/exit. */
    base_speed = (int16_t)(0.4f * (float)raw_speed + 0.6f * (float)g_app.current_target_speed);
    if (base_speed < target_min) {
        base_speed = target_min;
    }
    g_app.current_target_speed = base_speed;

    correction = LF_Control_UpdatePDWithParams(error,
                                               dt_s,
                                               base_speed,
                                               &g_app.pid,
                                               kp,
                                               kd,
                                               kff,
                                               target_max_corr);
    LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);
    LF_Chassis_SetCommand(lf_limit_degraded_speed(left_cmd), lf_limit_degraded_speed(right_cmd));
    update_trusted_direction();
}

/* ==================== Ultrasonic Overhead Hold ==================== */

/**
 * @brief Process the ULTRASONIC_HOLD state.
 *
 * When an overhead obstacle is detected, the car:
 * 1. Stops immediately
 * 2. Sends a LoRa checkpoint message (once)
 * 3. Holds for the configured duration
 * 4. Resumes normal line following
 *
 * @param now_ms Current timestamp.
 */
static void process_ultrasonic_hold(uint32_t now_ms)
{
    LF_Chassis_Stop();

    /* Send LoRa message once during the hold. */
    if (!g_app.ultrasonic_lora_sent) {
        LF_Hook_OnReservedCheckpoint(g_lf_config.ultrasonic_lora_checkpoint_id);
        g_app.ultrasonic_lora_sent = true;
    }

    /* Resume after the hold duration expires. */
    if (lf_time_reached(now_ms, g_app.ultrasonic_hold_start_ms, g_lf_config.ultrasonic_hold_ms)) {
        /* Store cooldown start time; use lf_time_reached for wrapping-safe check. */
        g_app.ultrasonic_cooldown_until_ms = now_ms;
        LF_Control_ResetPid(&g_app.pid);
        lf_set_state(LF_APP_STATE_RUNNING);
    }
}

/* ==================== Running State ==================== */

/**
 * @brief Process the RUNNING state: the main line-following loop.
 *
 * Priority order:
 * 1. Fixed turn (from route script or right-angle fallback)
 * 2. Line loss (grace ticks then recovery)
 * 3. Radar obstacle avoidance
 * 4. Reorientation (if fixed turn disabled)
 * 5. Normal PD control
 *
 * @param now_ms Current timestamp.
 * @param dt_s   Time delta in seconds.
 */
static void process_running(uint32_t now_ms, float dt_s)
{
    int8_t right_angle_side;
    bool reorient_cooling;

    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();
    LF_App_DetectSegmentType();
    LF_App_UpdateDirectionHistory();

    /* 1. Check for a fixed turn (route script or right-angle fallback). */
    if (LF_App_TryStartFixedTurn(now_ms)) {
        return;
    }

    /* 2. Handle line loss. */
    if (!g_app.last_frame.line_detected) {
        lf_bump_u8(&g_app.line_lost_count);
        if (g_app.line_lost_count <= g_lf_config.line_lost_grace_ticks) {
            hold_last_line_direction();
            return;
        }
        start_recovery(now_ms);
        return;
    }
    g_app.line_lost_count = 0U;

    /* 3. Radar obstacle avoidance. */
    if (g_lf_config.obstacle_avoid_enable && g_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        LF_App_StartAvoidance(now_ms);
        return;
    }

    /* 3.5 Ultrasonic overhead obstacle detection. */
    if (g_lf_config.ultrasonic_enable) {
        const LF_UltrasonicReading *us = LF_Ultrasonic_GetReading();
        /* Cooldown uses wrapping-safe lf_time_reached. */
        bool cooldown_active = g_app.ultrasonic_cooldown_until_ms > 0U &&
                               !lf_time_reached(now_ms, g_app.ultrasonic_cooldown_until_ms,
                                                g_lf_config.ultrasonic_cooldown_ms);
        if (us->state == LF_ULTRASONIC_BLOCK && !cooldown_active) {
            g_app.ultrasonic_hold_start_ms = now_ms;
            g_app.ultrasonic_lora_sent = false;
            lf_set_state(LF_APP_STATE_ULTRASONIC_HOLD);
            return;
        }
    }

    /* 4. Reorientation (only when fixed turn is disabled). */
    reorient_cooling = g_lf_config.reorient_cooldown_ms > 0U &&
        g_app.reorient_last_finish_ms > 0U &&
        (uint32_t)(now_ms - g_app.reorient_last_finish_ms) < g_lf_config.reorient_cooldown_ms;
    right_angle_side = LF_App_DetectRightAngleSide();
    if (g_lf_config.reorient_enable && !g_lf_config.fixed_turn_enable && !reorient_cooling &&
        right_angle_side != 0 && lf_abs_i32(g_app.last_frame.position) >= g_lf_config.reorient_position_threshold) {
        LF_App_StartReorient(now_ms, right_angle_side);
        return;
    }

    /* 5. Normal PD line following. */
    run_pd_control(dt_s);
}

/* ==================== Startup & Calibration ==================== */

/**
 * @brief Wait until the car detects a line for a sustained period.
 * @param now_ms Current timestamp.
 * @return true if the line has been held for start_line_hold_ms.
 */
static bool wait_start_line_ready(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - g_app.boot_ms) < g_lf_config.start_min_boot_delay_ms) {
        g_app.wait_start_line_since_ms = 0U;
        return false;
    }

    LF_Sensor_ReadFrame(&g_app.last_frame);
    if (!g_app.last_frame.line_detected) {
        g_app.wait_start_line_since_ms = 0U;
        return false;
    }

    if (g_app.wait_start_line_since_ms == 0U) {
        g_app.wait_start_line_since_ms = now_ms;
    }
    return (uint32_t)(now_ms - g_app.wait_start_line_since_ms) >= g_lf_config.start_line_hold_ms;
}

/**
 * @brief Begin sensor calibration.
 * @param now_ms Current timestamp.
 */
static void start_calibration(uint32_t now_ms)
{
    LF_Sensor_StartCalibration();
    g_app.calib_start_ms = now_ms;
    g_app.wait_start_line_since_ms = 0U;
    lf_set_state(LF_APP_STATE_CALIBRATING);
}

/**
 * @brief Run calibration motion (alternating spin) or stay still for fast calibration.
 * @param now_ms Current timestamp.
 */
static void run_calibration_motion(uint32_t now_ms)
{
    uint32_t elapsed;
    uint32_t slot;
    int16_t spin;

    if (g_lf_config.sensor_fast_calibration) {
        LF_Chassis_Stop();
        return;
    }

    /* Alternate spin direction every calibration_switch_interval_ms. */
    elapsed = now_ms - g_app.calib_start_ms;
    slot = elapsed / g_lf_config.calibration_switch_interval_ms;
    spin = g_lf_config.calibration_spin_speed;

    if ((slot & 0x1U) == 0U) {
        LF_Chassis_SetCommand(spin, (int16_t)(-spin));
    } else {
        LF_Chassis_SetCommand((int16_t)(-spin), spin);
    }
}

/* ==================== Public API ==================== */

void LF_App_Init(void)
{
    uint32_t now = LF_Platform_GetMillis();

    memset(&g_app, 0, sizeof(g_app));
    LF_Radar_Init();
    LF_Sensor_Init();
    LF_Ultrasonic_Init();
    LF_Control_ResetPid(&g_app.pid);
    LF_Chassis_Init();

    g_app.boot_ms = now;
    g_app.last_step_us = LF_Platform_GetMicros();
    g_app.last_seen_dir = +1;
    g_app.trusted_line_dir = +1;
    g_app.current_target_speed = g_lf_config.base_speed;
    g_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    g_app.route_phase = (uint8_t)LF_ROUTE_PHASE_WAIT_ARM;
    g_app.route_cross_armed = true;
    g_app.segment_type = LF_SEGMENT_STRAIGHT;
    g_app.segment_candidate = LF_SEGMENT_STRAIGHT;
    g_app.fixed_turn_event = (uint8_t)LF_ROUTE_EVENT_NONE;
    s_route_pending_phase = 0xFFU;

    lf_set_state(LF_APP_STATE_WAIT_START);
}

void LF_App_RunStep(void)
{
    uint32_t now_us = LF_Platform_GetMicros();
    uint32_t now_ms = LF_Platform_GetMillis();
    float dt_s;

    /* Enforce the control period; skip if not enough time has elapsed.
     * Guard against control_period_ms=0 which would cause dt_s=0 and
     * RunStep firing every call at maximum rate. */
    uint32_t period_us = (uint32_t)g_lf_config.control_period_ms * 1000U;
    if (period_us == 0U) {
        period_us = 1000U;  /* Minimum 1ms period. */
    }
    if (!lf_time_reached(now_us, g_app.last_step_us, period_us)) {
        return;
    }

    dt_s = (float)(now_us - g_app.last_step_us) / 1000000.0f;
    g_app.last_step_us = now_us;
    LF_App_RefreshRadarSnapshot(now_ms);
    LF_Ultrasonic_Tick(now_ms);

    switch (g_app.state) {
    case LF_APP_STATE_WAIT_START:
        LF_Chassis_Stop();
        if (LF_Platform_IsStartButtonPressed() ||
            (g_lf_config.auto_start_delay_ms > 0U &&
             (uint32_t)(now_ms - g_app.boot_ms) >= g_lf_config.auto_start_delay_ms) ||
            wait_start_line_ready(now_ms)) {
            start_calibration(now_ms);
        }
        break;

    case LF_APP_STATE_CALIBRATING:
        run_calibration_motion(now_ms);
        LF_Sensor_UpdateCalibration();
        if (lf_time_reached(now_ms, g_app.calib_start_ms, g_lf_config.calibration_duration_ms)) {
            const LF_SensorCalibration *calib;
            LF_Chassis_Stop();
            LF_Sensor_EndCalibration();
            calib = LF_Sensor_GetCalibration();
            g_app.calibration_ok = calib->calibrated;
            g_app.calibration_degraded = (calib->status == LF_SENSOR_CAL_DEGRADED);
            LF_Hook_OnCalibrationComplete(g_app.calibration_ok);
            if (!g_app.calibration_ok) {
                lf_set_state(LF_APP_STATE_FAULT);
                break;
            }
            g_app.run_start_ms = now_ms;
            g_app.current_target_speed = g_lf_config.base_speed;
            LF_Control_ResetPid(&g_app.pid);
            lf_set_state(LF_APP_STATE_RUNNING);
        }
        break;

    case LF_APP_STATE_RUNNING:
        process_running(now_ms, dt_s);
        break;

    case LF_APP_STATE_RECOVERING:
        process_recovery(now_ms);
        break;

    case LF_APP_STATE_AVOID_PREP:
        LF_App_ProcessAvoidPrep(now_ms);
        break;

    case LF_APP_STATE_AVOID_BYPASS:
        LF_App_ProcessAvoidBypass(now_ms);
        break;

    case LF_APP_STATE_AVOID_REACQUIRE:
        LF_App_ProcessAvoidReacquire(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_STOP:
        LF_App_ProcessFixedTurnStop(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_SPIN:
        LF_App_ProcessFixedTurnSpin(now_ms);
        break;

    case LF_APP_STATE_FIXED_TURN_SETTLE:
        LF_App_ProcessFixedTurnSettle(now_ms);
        break;

    case LF_APP_STATE_REORIENT_STOP:
        LF_App_ProcessReorientStop(now_ms);
        break;

    case LF_APP_STATE_REORIENT_SPIN:
        LF_App_ProcessReorientSpin(now_ms);
        break;

    case LF_APP_STATE_REORIENT_CONFIRM:
        LF_App_ProcessReorientConfirm(now_ms);
        break;

    case LF_APP_STATE_ULTRASONIC_HOLD:
        process_ultrasonic_hold(now_ms);
        break;

    case LF_APP_STATE_STOPPED:
        LF_Chassis_Stop();
        if (!g_app.run_finalized) {
            LF_LedBlink_SetPostmortemCode(1U, (uint8_t)(g_app.recovery_count > 15U ? 15U : g_app.recovery_count));
            g_app.run_finalized = true;
        }
        break;

    case LF_APP_STATE_BOOT:
    case LF_APP_STATE_FAULT:
    default:
        LF_Chassis_Stop();
        if (!g_app.run_finalized) {
            LF_LedBlink_SetPostmortemCode(3U, (uint8_t)(g_app.recovery_count > 15U ? 15U : g_app.recovery_count));
            g_app.run_finalized = true;
        }
        break;
    }

    LF_WatchDebug_UpdateApp(&g_app);
}

const LF_AppContext *LF_App_GetContext(void)
{
    return &g_app;
}

void LF_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    LF_Hook_OnReservedCheckpoint(checkpoint_id);
}
