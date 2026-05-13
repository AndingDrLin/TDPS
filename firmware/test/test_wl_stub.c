#include "wl_app.h"
#include "wl_config.h"
#include "wl_platform.h"

int main(void)
{
    if (!WL_App_Init()) {
        return 1;
    }

    WL_App_StartRace();
    WL_Platform_DelayMs(5000U);
    WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);

    for (unsigned i = 0U; i < 1000U; ++i) {
        WL_App_Tick();
        WL_Platform_DelayMs(1U);
    }

    WL_App_StopRace();
    return WL_App_GetState() == WL_APP_STATE_FINISHED ? 0 : 1;
}
