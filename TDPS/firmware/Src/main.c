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

extern void LF_Config_ApplyDebugProfile(void);

int main(void)
{
    bool wireless_ready;
    char profile_line[128];

    LF_Config_ApplyDebugProfile();  /* 低速整车巡线测试：线稳定后自动开始、快标定、雷达只观测 */
    LF_Platform_BoardInit();
    LF_LedBlink_Init();
    LF_RunLog_Init();
    LF_DebugMonitor_Init();
    wireless_ready = Wireless_Hooks_Init();
    LF_App_Init();
    LF_DgusScreen_Init();

    snprintf(profile_line,
             sizeof(profile_line),
             "Profile debug v2 invert=%u base=%d maxcorr=%d kp=%ld kd=%ld hold=%d/%d recover=%d/%d\n",
             g_lf_config.sensor_invert_polarity ? 1U : 0U,
             (int)g_lf_config.base_speed,
             (int)g_lf_config.max_correction,
             (long)(g_lf_config.kp * 1000.0f),
             (long)(g_lf_config.kd * 1000.0f),
             (int)g_lf_config.line_hold_speed,
             (int)g_lf_config.line_hold_turn_speed,
             (int)g_lf_config.recover_backtrack_speed,
             (int)g_lf_config.recover_turn_speed);
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
