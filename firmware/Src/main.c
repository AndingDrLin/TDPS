#include <stdbool.h>

#include "lf_app.h"
#include "lf_platform.h"
#include "wireless_hooks.h"

int main(void)
{
    bool wireless_ready;

    LF_Platform_BoardInit();
    wireless_ready = Wireless_Hooks_Init();
    LF_App_Init();

    if (wireless_ready) {
        LF_Platform_DebugPrint("System ready: line-follow + wireless\n");
    } else {
        LF_Platform_DebugPrint("System ready: line-follow only\n");
    }

    while (1) {
        LF_App_RunStep();
        Wireless_Hooks_Tick();
        LF_Platform_DelayMs(1U);
    }
}
