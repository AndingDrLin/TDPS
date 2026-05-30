#include <stdbool.h>
#include <stdio.h>

#include "lf_app.h"
#include "lf_config.h"
#include "lf_debug_monitor.h"
#include "lf_dgus_screen.h"
#include "lf_led_blink.h"
#include "lf_platform.h"
#include "lf_run_log.h"
#include "wireless_hooks.h"

extern void LF_Config_ApplyTrackProfile(void);

int main(void)
{
    bool wireless_ready;
    char profile_line[192];

    LF_Config_ApplyTrackProfile();
    LF_Platform_BoardInit();
    LF_LedBlink_Init();
    LF_RunLog_Init();
    LF_DebugMonitor_Init();
    wireless_ready = Wireless_Hooks_Init();
    LF_App_Init();
    LF_DgusScreen_Init();

    snprintf(profile_line,
             sizeof(profile_line),
             "Profile track invert=%u base=%d boost=%d curve=%d kp=%ld kd=%ld straight_dead=%d scale=%u limit=%d fork=%u hold=%d/%d recover=%d/%d\n",
             g_lf_config.sensor_invert_polarity ? 1U : 0U,
             (int)g_lf_config.base_speed,
             (int)g_lf_config.straight_boost_speed,
             (int)g_lf_config.curve_prepare_speed,
             (long)(g_lf_config.kp * 1000.0f),
             (long)(g_lf_config.kd * 1000.0f),
             (int)g_lf_config.straight_error_deadband,
             (unsigned)g_lf_config.straight_error_scale_percent,
             (int)g_lf_config.straight_correction_limit,
             g_lf_config.fork_enable ? 1U : 0U,
             (int)g_lf_config.line_hold_speed,
             (int)g_lf_config.line_hold_turn_speed,
             (int)g_lf_config.recover_sweep_start_speed,
             (int)g_lf_config.recover_sweep_max_speed);
    LF_Platform_DebugPrint(profile_line);

    if (wireless_ready) {
        LF_Platform_DebugPrint("System ready: line-follow + wireless\n");
    } else {
        LF_Platform_DebugPrint("System ready: line-follow only\n");
    }

    while (1) {
        LF_App_RunStep();
        LF_LedBlink_Tick();
        LF_DgusScreen_Tick();
        Wireless_Hooks_Tick();
        LF_DebugMonitor_Tick();
        LF_Platform_DelayMs(1U);
    }
}
