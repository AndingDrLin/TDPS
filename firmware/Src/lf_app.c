#include "lf_app.h"

#include <stddef.h>
#include <stdio.h>

#include "lf_chassis.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_future_hooks.h"
#include "lf_platform.h"
#include "lf_radar.h"
#include "lf_sensor.h"

static LF_AppContext s_app;

static bool time_reached(uint32_t now, uint32_t last, uint32_t period)
{
    return ((uint32_t)(now - last) >= period);
}

static void set_state(LF_AppState state)
{
    s_app.state = state;
}

static void refresh_radar_snapshot(uint32_t now_ms)
{
    const LF_RadarState *radar;

    LF_Radar_Tick(now_ms);
    radar = LF_Radar_GetState();

    s_app.obstacle_state = radar->obstacle_state;
    s_app.obstacle_distance_mm = radar->distance_mm;
    s_app.radar_parse_error_count = radar->parse_error_count;
}

static void run_calibration_motion(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - s_app.calib_start_ms;
    uint32_t slot = elapsed / g_lf_config.calibration_switch_interval_ms;
    int16_t spin = g_lf_config.calibration_spin_speed;

    /*
     * 标定期间原地左右摆动：
     * 目的不是“跑起来”，而是让传感器视野尽量覆盖黑白区域，提升 min/max 质量。
     */
    if ((slot & 0x1U) == 0U) {
        LF_Chassis_SetCommand(spin, (int16_t)(-spin));
    } else {
        LF_Chassis_SetCommand((int16_t)(-spin), spin);
    }
}

static void process_running(float dt_s)
{
    int16_t correction;
    int16_t left_cmd;
    int16_t right_cmd;
    int16_t base_speed = g_lf_config.base_speed;
    float error;

    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        LF_Chassis_Stop();
        LF_Hook_OnReservedObstacleWindow();
        return;
    }

    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_WARN &&
        base_speed > g_lf_config.obstacle_warn_speed) {
        base_speed = g_lf_config.obstacle_warn_speed;
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);

    if (!s_app.last_frame.line_detected) {
        if (s_app.last_frame.edge_hint != 0) {
            s_app.last_seen_dir = s_app.last_frame.edge_hint;
        }
        s_app.recover_start_ms = LF_Platform_GetMillis();
        set_state(LF_APP_STATE_RECOVERING);
        LF_Hook_OnLineLost();
        return;
    }

    /*
     * position 左负右正，目标值是 0。
     * error > 0 代表需要向右修正，error < 0 代表需要向左修正。
     */
    error = (float)(-s_app.last_frame.position);
    correction = LF_Control_UpdatePid(error, dt_s, &s_app.pid);
    LF_Control_ComputeMotorCmd(base_speed, correction, &left_cmd, &right_cmd);

    if (s_app.last_frame.edge_hint != 0) {
        s_app.last_seen_dir = s_app.last_frame.edge_hint;
    } else if (s_app.last_frame.position < 0) {
        s_app.last_seen_dir = -1;
    } else if (s_app.last_frame.position > 0) {
        s_app.last_seen_dir = +1;
    }

    LF_Chassis_SetCommand(left_cmd, right_cmd);
}

static void process_recovery(uint32_t now_ms)
{
    int16_t turn = g_lf_config.recover_turn_speed;

    if (s_app.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        LF_Chassis_Stop();
        LF_Hook_OnReservedObstacleWindow();
        return;
    }

    if (s_app.last_seen_dir < 0) {
        /* 最后在线偏左，恢复阶段优先向该侧搜索，保持与 RUNNING 修正方向一致。 */
        LF_Chassis_SetCommand((int16_t)(-turn), turn);
    } else {
        LF_Chassis_SetCommand(turn, (int16_t)(-turn));
    }

    LF_Sensor_ReadFrame(&s_app.last_frame);
    if (s_app.last_frame.line_detected) {
        LF_Control_ResetPid(&s_app.pid);
        set_state(LF_APP_STATE_RUNNING);
        LF_Hook_OnLineRecovered();
        return;
    }

    if ((uint32_t)(now_ms - s_app.recover_start_ms) >= g_lf_config.recover_timeout_ms) {
        LF_Chassis_Stop();
        set_state(LF_APP_STATE_STOPPED);
        LF_Platform_DebugPrint("Recovery timeout -> STOP\n");
    }
}

void LF_App_Init(void)
{
    uint32_t now = LF_Platform_GetMillis();

    LF_Radar_Init();
    LF_Sensor_Init();
    LF_Control_ResetPid(&s_app.pid);
    LF_Chassis_Init();

    s_app.boot_ms = now;
    s_app.last_step_ms = now;
    s_app.calib_start_ms = 0U;
    s_app.recover_start_ms = 0U;
    s_app.last_seen_dir = +1;
    s_app.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    s_app.obstacle_distance_mm = 0U;
    s_app.radar_parse_error_count = 0U;
    s_app.calibration_ok = false;

    set_state(LF_APP_STATE_WAIT_START);
    LF_Platform_SetStatusLed(false);
}

void LF_App_RunStep(void)
{
    uint32_t now_ms = LF_Platform_GetMillis();
    float dt_s;

    if (!time_reached(now_ms, s_app.last_step_ms, g_lf_config.control_period_ms)) {
        return;
    }

    dt_s = (float)(now_ms - s_app.last_step_ms) / 1000.0f;
    s_app.last_step_ms = now_ms;
    refresh_radar_snapshot(now_ms);

    switch (s_app.state) {
        case LF_APP_STATE_WAIT_START:
            if (LF_Platform_IsStartButtonPressed() ||
                ((uint32_t)(now_ms - s_app.boot_ms) >= g_lf_config.auto_start_delay_ms)) {
                LF_Sensor_StartCalibration();
                s_app.calib_start_ms = now_ms;
                set_state(LF_APP_STATE_CALIBRATING);
                LF_Hook_OnCalibrationBegin();
                LF_Platform_DebugPrint("Calibration start\n");
            }
            break;

        case LF_APP_STATE_CALIBRATING:
            run_calibration_motion(now_ms);
            LF_Sensor_UpdateCalibration();

            if ((uint32_t)(now_ms - s_app.calib_start_ms) >= g_lf_config.calibration_duration_ms) {
                const LF_SensorCalibration *calib;

                LF_Chassis_Stop();
                LF_Sensor_EndCalibration();
                calib = LF_Sensor_GetCalibration();
                s_app.calibration_ok = calib->calibrated;
                LF_Hook_OnCalibrationComplete(s_app.calibration_ok);

                if (!s_app.calibration_ok) {
                    LF_Chassis_Stop();
                    set_state(LF_APP_STATE_FAULT);
                    LF_Platform_DebugPrint("Calibration failed -> FAULT\n");
                    return;
                }

                LF_Control_ResetPid(&s_app.pid);
                set_state(LF_APP_STATE_RUNNING);
                LF_Platform_SetStatusLed(true);
                LF_Platform_DebugPrint("Calibration end -> RUNNING\n");
            }
            break;

        case LF_APP_STATE_RUNNING:
            process_running(dt_s);
            break;

        case LF_APP_STATE_RECOVERING:
            process_recovery(now_ms);
            break;

        case LF_APP_STATE_STOPPED:
            LF_Chassis_Stop();
            break;

        case LF_APP_STATE_BOOT:
        case LF_APP_STATE_FAULT:
        default:
            LF_Chassis_Stop();
            set_state(LF_APP_STATE_FAULT);
            break;
    }
}

const LF_AppContext *LF_App_GetContext(void)
{
    return &s_app;
}

void LF_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    LF_Hook_OnReservedCheckpoint(checkpoint_id);
}
