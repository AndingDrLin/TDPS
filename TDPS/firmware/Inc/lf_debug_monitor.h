/**
 * @file lf_debug_monitor.h
 * @brief Lightweight periodic debug output for UART/SWO diagnostics.
 */
#ifndef LF_DEBUG_MONITOR_H
#define LF_DEBUG_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

/** Debug monitor runtime configuration. */
typedef struct {
    bool enabled;              /**< Master enable for debug output */
    bool no_car_mode;          /**< Bench debug mode (no motors active) */
    uint32_t report_period_ms; /**< Interval between reports (ms) */
    bool report_line_raw;      /**< Include raw sensor values in output */
    bool report_wireless;      /**< Include wireless status in output */
    bool report_radar;         /**< Include radar status in output */
} LF_DebugMonitorConfig;

extern LF_DebugMonitorConfig g_lf_debug_monitor_config;

/** @brief Initialize the debug monitor and its output timer. */
void LF_DebugMonitor_Init(void);

/**
 * @brief Non-blocking periodic tick: emit a debug report if the interval has elapsed.
 *
 * Call from the main while(1) loop.
 */
void LF_DebugMonitor_Tick(void);

/**
 * @brief Check whether debug monitor output is enabled.
 * @return true if enabled.
 */
bool LF_DebugMonitor_IsEnabled(void);

/**
 * @brief Check whether no-car (bench) mode is active.
 * @return true if no-car mode is enabled.
 */
bool LF_DebugMonitor_IsNoCarMode(void);

/**
 * @brief Cache the most recent motor commands for debug reporting.
 * @param left_cmd  Last left motor command.
 * @param right_cmd Last right motor command.
 */
void LF_DebugMonitor_OnMotorCommand(int16_t left_cmd, int16_t right_cmd);

/**
 * @brief Retrieve the most recently cached motor commands.
 * @param[out] left_cmd  Receives the left motor command (may be NULL).
 * @param[out] right_cmd Receives the right motor command (may be NULL).
 */
void LF_DebugMonitor_GetLastMotorCommand(int16_t *left_cmd, int16_t *right_cmd);

#endif /* LF_DEBUG_MONITOR_H */
