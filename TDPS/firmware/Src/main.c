#include <stdbool.h>
#include <stdio.h>

#include "lf_app.h"
#include "lf_config.h"
#include "lf_debug_monitor.h"
#include "lf_led_blink.h"
#include "lf_platform.h"
#include "wireless_hooks.h"

int main(void)
{
    bool wireless_ready;
    char profile_line[160];

    LF_Config_ApplyDebugProfile();
    LF_Platform_BoardInit();
    LF_LedBlink_Init();
    LF_DebugMonitor_Init();
    wireless_ready = Wireless_Hooks_Init();
    LF_App_Init();

    snprintf(profile_line,
             sizeof(profile_line),
             "Profile debug invert=%u base=%d kp=%ld kd=%ld route=%u fixed=%u segment=%u recover=%d\n",
             g_lf_config.sensor_invert_polarity ? 1U : 0U,
             (int)g_lf_config.base_speed,
             (long)(g_lf_config.kp * 1000.0f),
             (long)(g_lf_config.kd * 1000.0f),
             g_lf_config.route_script_enable ? 1U : 0U,
             g_lf_config.fixed_turn_enable ? 1U : 0U,
             g_lf_config.segment_control_enable ? 1U : 0U,
             (int)g_lf_config.recover_sweep_speed);
    LF_Platform_DebugPrint(profile_line);

    LF_Platform_DebugPrint(wireless_ready ?
                           "System ready: line-follow + wireless\n" :
                           "System ready: line-follow only\n");

    while (1) {
        LF_App_RunStep();
        LF_LedBlink_Tick();
        Wireless_Hooks_Tick();
        LF_DebugMonitor_Tick();
    }
}
