/**
 * @file wireless_hooks.c
 * @brief Wireless integration hooks for the line-following system.
 *
 * Overrides weak LF_Hook_* symbols to route calibration and checkpoint
 * events to the WL_App state machine. Disabled in MinGW stub builds.
 */

#include "wireless_hooks.h"

#include <stdint.h>

#include "lf_future_hooks.h"
#include "lf_platform.h"
#include "wl_app.h"

static bool s_wireless_ready = false;
static bool s_race_started = false;

/** @brief Initialize the wireless module; continues line-follow on failure. */
bool Wireless_Hooks_Init(void)
{
    s_wireless_ready = WL_App_Init();
    s_race_started = false;

    if (!s_wireless_ready) {
        LF_Platform_DebugPrint("Wireless init failed, continue line-follow only\n");
    }

    return s_wireless_ready;
}

/** @brief Reset wireless module to uninitialized state. */
void Wireless_Hooks_Reset(void)
{
    s_wireless_ready = false;
    s_race_started = false;
}

/** @brief Non-blocking tick that advances the wireless state machine. */
void Wireless_Hooks_Tick(void)
{
    if (!s_wireless_ready) {
        return;
    }

    WL_App_Tick();
}

/** @brief Check if the wireless module initialized successfully. */
bool Wireless_Hooks_IsReady(void)
{
    return s_wireless_ready;
}

#if !defined(__MINGW32__) && !defined(__MINGW64__)
/**
 * @brief Start the wireless race timer after successful calibration.
 * @param success true if calibration succeeded; ignored if already started.
 */
void LF_Hook_OnCalibrationComplete(bool success)
{
    if (!s_wireless_ready || !success || s_race_started) {
        return;
    }

    WL_App_StartRace();
    s_race_started = true;
}

/**
 * @brief Forward a checkpoint event to the wireless state machine.
 * @param checkpoint_id Identifier of the checkpoint reached.
 */
void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id)
{
    if (!s_wireless_ready || !s_race_started) {
        return;
    }

    WL_App_NotifyCheckpoint(checkpoint_id);
}
#endif
