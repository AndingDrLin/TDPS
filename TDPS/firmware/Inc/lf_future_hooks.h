/**
 * @file lf_future_hooks.h
 * @brief Reserved hooks for future peripheral integration.
 *
 * Default weak implementations are provided in lf_future_hooks.c.
 * Override by defining a non-weak function with the same signature
 * in another .c file (e.g., wireless_hooks.c).
 */
#ifndef LF_FUTURE_HOOKS_H
#define LF_FUTURE_HOOKS_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Called when sensor calibration begins. */
void LF_Hook_OnCalibrationBegin(void);

/**
 * @brief Called when sensor calibration completes.
 * @param success true if calibration succeeded.
 */
void LF_Hook_OnCalibrationComplete(bool success);

/** @brief Called when the line is lost. */
void LF_Hook_OnLineLost(void);

/** @brief Called when the line is recovered after being lost. */
void LF_Hook_OnLineRecovered(void);

/**
 * @brief Reserved: trigger LoRa send at intersections/waypoints.
 * @param checkpoint_id Identifier of the checkpoint.
 */
void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id);

/** @brief Reserved: trigger radar decision before an obstacle zone. */
void LF_Hook_OnReservedObstacleWindow(void);

#endif /* LF_FUTURE_HOOKS_H */
