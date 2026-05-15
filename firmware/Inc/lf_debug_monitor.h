#ifndef LF_DEBUG_MONITOR_H
#define LF_DEBUG_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool no_car_mode;
    uint32_t report_period_ms;
    bool report_line_raw;
    bool report_wireless;
    bool report_radar;
} LF_DebugMonitorConfig;

extern LF_DebugMonitorConfig g_lf_debug_monitor_config;

void LF_DebugMonitor_Init(void);
void LF_DebugMonitor_Tick(void);
bool LF_DebugMonitor_IsEnabled(void);
bool LF_DebugMonitor_IsNoCarMode(void);
void LF_DebugMonitor_OnMotorCommand(int16_t left_cmd, int16_t right_cmd);
void LF_DebugMonitor_GetLastMotorCommand(int16_t *left_cmd, int16_t *right_cmd);

#endif /* LF_DEBUG_MONITOR_H */
