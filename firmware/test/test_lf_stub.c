#include "lf_app.h"
#include "lf_platform.h"
#include "wireless_hooks.h"

int main(void)
{
    LF_Platform_BoardInit();
    (void)Wireless_Hooks_Init();
    LF_App_Init();

    for (unsigned i = 0U; i < 6000U; ++i) {
        LF_App_RunStep();
        Wireless_Hooks_Tick();
        LF_Platform_DelayMs(1U);
    }

    return LF_App_GetContext() == 0 ? 1 : 0;
}
