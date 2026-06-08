/**
 * @file lf_future_hooks.c
 * @brief Default weak implementations for event hooks.
 *
 * These are overridden at link time when a stronger definition is provided
 * (e.g., in wireless_hooks.c for LoRa integration).
 */

#include "lf_future_hooks.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
#define LF_WEAK
#elif defined(__GNUC__) || defined(__clang__) || defined(__ARMCC_VERSION)
#define LF_WEAK __attribute__((weak))
#elif defined(__CC_ARM)
#define LF_WEAK __weak
#else
#define LF_WEAK
#endif

/*
 * Default empty implementations (weak symbols).
 * Provide a non-weak function with the same name in another .c file to override.
 */

/** @brief Default no-op calibration begin hook. */
LF_WEAK void LF_Hook_OnCalibrationBegin(void) {}

/** @brief Default no-op calibration complete hook. */
LF_WEAK void LF_Hook_OnCalibrationComplete(bool success) { (void)success; }

/** @brief Default no-op line lost hook. */
LF_WEAK void LF_Hook_OnLineLost(void) {}

/** @brief Default no-op line recovered hook. */
LF_WEAK void LF_Hook_OnLineRecovered(void) {}

/** @brief Default no-op checkpoint hook. */
LF_WEAK void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id) { (void)checkpoint_id; }

/** @brief Default no-op obstacle window hook. */
LF_WEAK void LF_Hook_OnReservedObstacleWindow(void) {}
