#ifndef LF_RUN_LOG_H
#define LF_RUN_LOG_H

#include <stdbool.h>
#include <stdint.h>

#define LF_RUN_LOG_CAPACITY 64U

typedef enum {
    LF_LOG_STATE_CHANGE = 0,
    LF_LOG_CALIBRATION,
    LF_LOG_OBSTACLE,
    LF_LOG_FORK,
} LF_LogEventType;

typedef struct {
    uint16_t timestamp_ms;
    uint8_t  event_type;
    uint8_t  data_a;
    uint16_t data_b;
} LF_LogEntry;  /* 6 bytes */

typedef struct {
    LF_LogEntry entries[LF_RUN_LOG_CAPACITY];
    uint8_t  head;
    uint8_t  count;
    uint16_t total_recoveries;
    uint16_t total_obstacles;
    uint16_t total_forks;
    uint16_t run_duration_s;
    uint8_t  final_state;
    uint8_t  final_reason;
    uint8_t  cal_quality;
} LF_RunLog;

void LF_RunLog_Init(void);
void LF_RunLog_Record(uint8_t event_type, uint8_t data_a, uint16_t data_b);
void LF_RunLog_AddRecovery(void);
void LF_RunLog_Finalize(uint8_t final_state, uint8_t final_reason);
void LF_RunLog_SetCalQuality(uint8_t quality);
const LF_RunLog *LF_RunLog_Get(void);

#endif /* LF_RUN_LOG_H */
