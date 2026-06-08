/**
 * @file wireless_hooks.h
 * @brief Wireless integration entry point for the line-following system.
 */
#ifndef WIRELESS_HOOKS_H
#define WIRELESS_HOOKS_H

#include <stdbool.h>

/**
 * @brief Initialize the wireless module.
 * @return true on success; failure does not block the main line-following path.
 */
bool Wireless_Hooks_Init(void);

/** @brief Reset the wireless module state. */
void Wireless_Hooks_Reset(void);

/**
 * @brief Main-loop tick for wireless processing.
 *
 * Call this periodically from the main while(1) loop.
 */
void Wireless_Hooks_Tick(void);

/**
 * @brief Check if the wireless module is ready.
 * @return true if ready, false otherwise.
 */
bool Wireless_Hooks_IsReady(void);

#endif /* WIRELESS_HOOKS_H */
