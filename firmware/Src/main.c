#include <stdbool.h>

#include "lf_app.h"
#include "lf_debug_monitor.h"
#include "lf_dgus_screen.h"
#include "lf_platform.h"
#include "wireless_hooks.h"

int main(void)
{
    bool wireless_ready;

    LF_Platform_BoardInit();
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
        LF_DgusScreen_Tick();
        Wireless_Hooks_Tick();
        LF_DebugMonitor_Tick();
        LF_Platform_DelayMs(1U);
    }
}
