/**
 * @file lf_app_internal.h
 * @brief Shared internal state for the line-following application modules.
 *
 * This header is only included by lf_app*.c files. It exposes the
 * application context and helper accessors so that split modules can
 * operate on shared state without making it globally visible.
 */
#ifndef LF_APP_INTERNAL_H
#define LF_APP_INTERNAL_H

#include "lf_app.h"

/**
 * @brief Main application context shared across all lf_app_* modules.
 *
 * Defined in lf_app.c. All sub-modules access it through this extern.
 */
extern LF_AppContext g_app;

/**
 * @brief Get a pointer to the digital sensor cache.
 * @return Pointer to the uint8_t[LF_SENSOR_COUNT] digital cache array.
 */
uint8_t *LF_App_GetDigitalCache(void);

/**
 * @brief Check whether the digital cache was successfully refreshed.
 * @return true if the cache contains valid data from the latest UART frame.
 */
bool LF_App_IsDigitalCacheValid(void);

/**
 * @brief Mark the digital cache as valid or invalid.
 * @param valid true if the cache was just refreshed successfully.
 */
void LF_App_SetDigitalCacheValid(bool valid);

/**
 * @brief Get a pointer to the pending route phase variable.
 * @return Pointer to the pending route phase (0xFF means no pending).
 */
uint8_t *LF_App_GetRoutePendingPhase(void);

#endif /* LF_APP_INTERNAL_H */
