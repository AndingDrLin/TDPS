#include <stdbool.h>

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

    LF_Platform_BoardInit();
    LF_Config_ApplyDebugProfile();  /* 低速整车巡线测试：线稳定后自动开始、快标定、雷达只观测 */
    LF_LedBlink_Init();
    LF_RunLog_Init();
    LF_DebugMonitor_Init();
    wireless_ready = Wireless_Hooks_Init();
    LF_App_Init();
    LF_DgusScreen_Init();

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
