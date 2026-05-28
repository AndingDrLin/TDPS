#include "lf_run_log.h"

#include "lf_platform.h"

static LF_RunLog s_log;

void LF_RunLog_Init(void)
{
    uint32_t i;

    s_log.head = 0U;
    s_log.count = 0U;
    s_log.total_recoveries = 0U;
    s_log.total_obstacles = 0U;
    s_log.total_forks = 0U;
    s_log.run_duration_s = 0U;
    s_log.final_state = 0U;
    s_log.final_reason = 0U;
    s_log.cal_quality = 0U;
    for (i = 0U; i < LF_RUN_LOG_CAPACITY; ++i) {
        s_log.entries[i].timestamp_ms = 0U;
        s_log.entries[i].event_type = 0U;
        s_log.entries[i].data_a = 0U;
        s_log.entries[i].data_b = 0U;
    }
}

void LF_RunLog_Record(uint8_t event_type, uint8_t data_a, uint16_t data_b)
{
    LF_LogEntry *entry;

    entry = &s_log.entries[s_log.head];
    entry->timestamp_ms = (uint16_t)(LF_Platform_GetMillis() & 0xFFFFU);
    entry->event_type = event_type;
    entry->data_a = data_a;
    entry->data_b = data_b;

    s_log.head = (uint8_t)((s_log.head + 1U) % LF_RUN_LOG_CAPACITY);
    if (s_log.count < LF_RUN_LOG_CAPACITY) {
        s_log.count++;
    }

    /* Update running counters. */
    if (event_type == LF_LOG_OBSTACLE) {
        s_log.total_obstacles++;
    } else if (event_type == LF_LOG_FORK) {
        s_log.total_forks++;
    }
}

void LF_RunLog_SetCalQuality(uint8_t quality)
{
    s_log.cal_quality = quality;
}

void LF_RunLog_Finalize(uint8_t final_state, uint8_t final_reason)
{
    s_log.final_state = final_state;
    s_log.final_reason = final_reason;
    s_log.run_duration_s = (uint16_t)((LF_Platform_GetMillis() / 1000U) & 0xFFFFU);
}

const LF_RunLog *LF_RunLog_Get(void)
{
    return &s_log;
}

/* Increment recovery counter. Called from lf_app.c. */
void LF_RunLog_AddRecovery(void)
{
    s_log.total_recoveries++;
}
