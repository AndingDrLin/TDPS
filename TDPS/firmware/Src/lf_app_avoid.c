/**
 * @file lf_app_avoid.c
 * @brief Obstacle avoidance, fixed 90-degree turns, and reorientation.
 *
 * Contains three state-machine sub-systems:
 *
 * 1. Obstacle avoidance (radar-triggered):
 *    AVOID_PREP -> AVOID_BYPASS (3-phase arc) -> AVOID_REACQUIRE (sweep)
 *
 * 2. Fixed 90-degree turn (route-script-triggered):
 *    FIXED_TURN_STOP -> FIXED_TURN_SPIN -> FIXED_TURN_SETTLE
 *
 * 3. Reorientation (right-angle-triggered, when fixed turn disabled):
 *    REORIENT_STOP -> REORIENT_SPIN -> REORIENT_CONFIRM
 */
#include "lf_app_avoid.h"

#include "lf_app_internal.h"
#include "lf_app_route.h"
#include "lf_app_sensor.h"
#include "lf_app_util.h"
#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"

/* ==================== Radar Snapshot ==================== */

void LF_App_RefreshRadarSnapshot(uint32_t now_ms)
{
    const LF_RadarState *radar;

    if (!g_lf_config.radar_enable) {
        g_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
        g_app.obstacle_distance_mm = 0U;
        g_app.radar_has_target = false;
        g_app.radar_frame_valid = false;
        return;
    }

    LF_Radar_Tick(now_ms);
    radar = LF_Radar_GetState();
    g_app.obstacle_state = radar->obstacle_state;
    g_app.obstacle_distance_mm = radar->distance_mm;
    g_app.radar_has_target = radar->has_target;
    g_app.radar_frame_valid = radar->frame_valid;
    g_app.radar_frame_count = radar->frame_count;
    g_app.radar_last_update_ms = radar->last_update_ms;
    g_app.radar_frame_age_ms = radar->frame_valid ? (uint32_t)(now_ms - radar->last_update_ms) : 0U;
}

/* ==================== Obstacle Avoidance ==================== */

int8_t LF_App_ChooseAvoidDir(void)
{
    /* Config-preferred side takes priority; otherwise use last seen direction. */
    if (g_lf_config.obstacle_preferred_side < 0) {
        return -1;
    }
    if (g_lf_config.obstacle_preferred_side > 0) {
        return +1;
    }
    return (g_app.last_seen_dir < 0) ? -1 : +1;
}

void LF_App_StartAvoidance(uint32_t now_ms)
{
    LF_Chassis_Stop();
    g_app.avoid_dir = LF_App_ChooseAvoidDir();
    g_app.avoid_confirm_count = 0U;
    g_app.avoid_state_start_ms = now_ms;
    lf_set_state(LF_APP_STATE_AVOID_PREP);
}

void LF_App_ProcessAvoidPrep(uint32_t now_ms)
{
    LF_Chassis_Stop();

    /* If the obstacle clears during prep, resume normal running. */
    if (g_app.obstacle_state != LF_RADAR_OBSTACLE_BLOCK) {
        LF_Control_ResetPid(&g_app.pid);
        lf_set_state(LF_APP_STATE_RUNNING);
        return;
    }

    if (!lf_time_reached(now_ms, g_app.avoid_state_start_ms, g_lf_config.obstacle_stop_ms)) {
        return;
    }

    g_app.avoid_state_start_ms = now_ms;
    lf_set_state(LF_APP_STATE_AVOID_BYPASS);
}

void LF_App_ProcessAvoidBypass(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - g_app.avoid_state_start_ms;
    uint32_t third = g_lf_config.obstacle_bypass_ms / 3U;
    int16_t turn = g_lf_config.obstacle_turn_speed;
    int16_t speed = g_lf_config.obstacle_bypass_speed;

    if (third == 0U) {
        third = 1U;
    }

    /* 3-phase arc maneuver:
     * Phase 1 (0-33%): sharp turn away from the line
     * Phase 2 (33-66%): forward with slight bias
     * Phase 3 (66-100%): sharp turn back toward the line */
    if (elapsed < third) {
        if (g_app.avoid_dir < 0) {
            LF_Chassis_SetCommand((int16_t)(-turn), turn);
        } else {
            LF_Chassis_SetCommand(turn, (int16_t)(-turn));
        }
    } else if (elapsed < (third * 2U)) {
        if (g_app.avoid_dir < 0) {
            LF_Chassis_SetCommand((int16_t)(speed / 2), speed);
        } else {
            LF_Chassis_SetCommand(speed, (int16_t)(speed / 2));
        }
    } else if (elapsed < g_lf_config.obstacle_bypass_ms) {
        if (g_app.avoid_dir < 0) {
            LF_Chassis_SetCommand(turn, (int16_t)(-turn));
        } else {
            LF_Chassis_SetCommand((int16_t)(-turn), turn);
        }
    } else {
        g_app.avoid_state_start_ms = now_ms;
        g_app.avoid_confirm_count = 0U;
        lf_set_state(LF_APP_STATE_AVOID_REACQUIRE);
    }
}

void LF_App_ProcessAvoidReacquire(uint32_t now_ms)
{
    int16_t turn = g_lf_config.recover_sweep_speed;
    uint32_t elapsed = now_ms - g_app.avoid_state_start_ms;
    bool sweep_left;

    /* Sweep in the avoidance direction first, then alternate after half the timeout. */
    sweep_left = (g_app.avoid_dir < 0);
    if (elapsed > (g_lf_config.obstacle_reacquire_timeout_ms / 2U)) {
        sweep_left = !sweep_left;  /* Alternate direction in the second half. */
    }

    /* Sweep until the line is re-acquired. */
    if (LF_App_LineConfirmed(&g_app.avoid_confirm_count, 3U)) {
        LF_Control_ResetPid(&g_app.pid);
        lf_set_state(LF_APP_STATE_RUNNING);
        return;
    }

    if (sweep_left) {
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    } else {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    }

    /* If re-acquisition times out, fall back to full recovery. */
    if (lf_time_reached(now_ms, g_app.avoid_state_start_ms, g_lf_config.obstacle_reacquire_timeout_ms)) {
        g_app.recover_start_ms = now_ms;
        g_app.recover_phase_start_ms = now_ms;
        g_app.recover_phase = (uint8_t)LF_RECOVER_SWEEP_LEFT;
        g_app.recover_confirm_count = 0U;
        lf_set_state(LF_APP_STATE_RECOVERING);
    }
}

/* ==================== Fixed 90-Degree Turn ==================== */

/**
 * @brief Get the spin duration for a fixed turn (direction-dependent).
 * @return Duration in milliseconds.
 */
static uint16_t fixed_turn_duration(void)
{
    return (g_app.fixed_turn_dir < 0) ?
        g_lf_config.fixed_turn_90_ms_left :
        g_lf_config.fixed_turn_90_ms_right;
}

/**
 * @brief Command a differential spin in the given direction.
 * @param dir   -1 for left spin, +1 for right spin.
 * @param speed Spin speed (absolute value).
 */
static void set_spin_command(int8_t dir, int16_t speed)
{
    if (dir > 0) {
        LF_Chassis_SetCommand(speed, (int16_t)(-speed));
    } else {
        LF_Chassis_SetCommand((int16_t)(-speed), speed);
    }
}

/**
 * @brief Initiate a fixed turn sequence.
 * @param now_ms Current timestamp.
 * @param dir    Turn direction (-1 left, +1 right).
 * @param event  Route event type for telemetry.
 */
static void start_fixed_turn(uint32_t now_ms, int8_t dir, uint8_t event)
{
    if (!g_lf_config.fixed_turn_enable || dir == 0) {
        return;
    }

    LF_App_RouteCommitPending();
    LF_Control_ResetPid(&g_app.pid);
    g_app.fixed_turn_dir = dir;
    g_app.fixed_turn_event = event;
    g_app.fixed_turn_phase_start_ms = now_ms;

    if (g_lf_config.fixed_turn_stop_ms == 0U) {
        /* Skip the stop phase; go directly to spin. */
        set_spin_command(g_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        lf_set_state(LF_APP_STATE_FIXED_TURN_SPIN);
    } else {
        LF_Chassis_Stop();
        lf_set_state(LF_APP_STATE_FIXED_TURN_STOP);
    }
}

bool LF_App_TryStartFixedTurn(uint32_t now_ms)
{
    int8_t dir = 0;
    uint8_t event = (uint8_t)LF_ROUTE_EVENT_NONE;

    if (!g_lf_config.fixed_turn_enable) {
        return false;
    }

    /* First check the route script for an expected turn. */
    if (LF_App_RouteScriptPeek(&dir, &event)) {
        if (dir != 0) {
            start_fixed_turn(now_ms, dir, event);
            return true;
        }
        /* Route script matched but says go straight (e.g. CROSS phase).
         * Do NOT suppress the right-angle fallback — the car might still
         * benefit from a right-angle turn if the script misidentified. */
    }

    /* Fallback: if no route script match, try right-angle detection. */
    if (!LF_App_FixedTurnCooldownActive(now_ms)) {
        dir = LF_App_DetectRightAngleSide();
        if (dir != 0 && lf_abs_i32(g_app.last_frame.position) >= 300) {
            start_fixed_turn(now_ms, dir, (uint8_t)LF_ROUTE_EVENT_NONE);
            return true;
        }
    }

    return false;
}

void LF_App_ProcessFixedTurnStop(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();
    LF_Chassis_Stop();

    if (lf_time_reached(now_ms, g_app.fixed_turn_phase_start_ms, g_lf_config.fixed_turn_stop_ms)) {
        g_app.fixed_turn_phase_start_ms = now_ms;
        set_spin_command(g_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        lf_set_state(LF_APP_STATE_FIXED_TURN_SPIN);
    }
}

void LF_App_ProcessFixedTurnSpin(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();

    /* Safety timeout: if spin takes more than 2x the expected duration,
     * something went wrong — stop and settle. */
    if (lf_time_reached(now_ms, g_app.fixed_turn_phase_start_ms,
                        (uint32_t)fixed_turn_duration() * 2U)) {
        LF_Chassis_Stop();
        g_app.fixed_turn_phase_start_ms = now_ms;
        lf_set_state(LF_APP_STATE_FIXED_TURN_SETTLE);
        return;
    }

    if (!lf_time_reached(now_ms, g_app.fixed_turn_phase_start_ms, fixed_turn_duration())) {
        set_spin_command(g_app.fixed_turn_dir, g_lf_config.fixed_turn_spin_speed);
        return;
    }

    LF_Chassis_Stop();
    g_app.fixed_turn_phase_start_ms = now_ms;
    lf_set_state(LF_APP_STATE_FIXED_TURN_SETTLE);
}

void LF_App_ProcessFixedTurnSettle(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();
    LF_Chassis_Stop();

    if (!lf_time_reached(now_ms, g_app.fixed_turn_phase_start_ms, g_lf_config.fixed_turn_settle_ms)) {
        return;
    }

    /* Turn complete: reset state and resume running at minimum speed. */
    g_app.route_last_event_ms = now_ms;
    g_app.fixed_turn_dir = 0;
    g_app.fixed_turn_event = (uint8_t)LF_ROUTE_EVENT_NONE;
    g_app.current_target_speed = g_lf_config.min_speed;
    LF_Control_ResetPid(&g_app.pid);
    lf_set_state(LF_APP_STATE_RUNNING);
}

/* ==================== Reorientation ==================== */

void LF_App_StartReorient(uint32_t now_ms, int8_t dir)
{
    LF_Control_ResetPid(&g_app.pid);
    g_app.reorient_spin_dir = dir;
    g_app.reorient_start_ms = now_ms;
    g_app.reorient_confirm_count = 0U;
    LF_Chassis_Stop();
    lf_set_state(LF_APP_STATE_REORIENT_STOP);
}

void LF_App_ProcessReorientStop(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();
    LF_Chassis_Stop();

    /* Fixed 60ms settle to avoid false confirmation from lingering middle channels. */
    if (lf_time_reached(now_ms, g_app.reorient_start_ms, 60U)) {
        g_app.reorient_start_ms = now_ms;
        set_spin_command(g_app.reorient_spin_dir, g_lf_config.reorient_spin_speed);
        lf_set_state(LF_APP_STATE_REORIENT_SPIN);
    }
}

void LF_App_ProcessReorientSpin(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();

    /* Timeout: stop the car if reorientation takes too long. */
    if (lf_time_reached(now_ms, g_app.reorient_start_ms, g_lf_config.reorient_timeout_ms)) {
        LF_Chassis_Stop();
        lf_set_state(LF_APP_STATE_STOPPED);
        return;
    }

    if (LF_App_MiddleSensorsAligned()) {
        g_app.reorient_confirm_count = 1U;
        LF_Chassis_Stop();
        lf_set_state(LF_APP_STATE_REORIENT_CONFIRM);
        return;
    }

    set_spin_command(g_app.reorient_spin_dir, g_lf_config.reorient_spin_speed);
}

void LF_App_ProcessReorientConfirm(uint32_t now_ms)
{
    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();
    LF_Chassis_Stop();

    /* Lost alignment: go back to spinning. */
    if (!LF_App_MiddleSensorsAligned()) {
        lf_set_state(LF_APP_STATE_REORIENT_SPIN);
        return;
    }

    lf_bump_u8(&g_app.reorient_confirm_count);
    if (g_app.reorient_confirm_count >= g_lf_config.reorient_confirm_ticks) {
        g_app.reorient_last_finish_ms = now_ms;
        LF_Control_ResetPid(&g_app.pid);
        lf_set_state(LF_APP_STATE_RUNNING);
    }
}
