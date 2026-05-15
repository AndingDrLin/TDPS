#include "wireless_hooks.h"

#include <stdint.h>

#include "lf_future_hooks.h"
#include "lf_platform.h"
#include "wl_app.h"

static bool s_wireless_ready = false;
static bool s_race_started = false;

bool Wireless_Hooks_Init(void)
{
    s_wireless_ready = WL_App_Init();
    s_race_started = false;

    if (!s_wireless_ready) {
        LF_Platform_DebugPrint("Wireless init failed, continue line-follow only\n");
    }

    return s_wireless_ready;
}

void Wireless_Hooks_Reset(void)
{
    s_wireless_ready = false;
    s_race_started = false;
}

void Wireless_Hooks_Tick(void)
{
    if (!s_wireless_ready) {
        return;
    }

    WL_App_Tick();
}

bool Wireless_Hooks_IsReady(void)
{
    return s_wireless_ready;
}

void LF_Hook_OnCalibrationComplete(bool success)
{
    if (!s_wireless_ready || !success || s_race_started) {
        return;
    }

    WL_App_StartRace();
    s_race_started = true;
}

void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id)
{
    if (!s_wireless_ready || !s_race_started) {
        return;
    }

    WL_App_NotifyCheckpoint(checkpoint_id);
}
