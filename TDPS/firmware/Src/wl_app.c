/**
 * @file    wl_app.c
 * @brief   Wireless communication application layer -- state machine,
 *          timing, and checkpoint message transmission.
 */

#include "wl_app.h"
#include "wl_config.h"
#include "wl_lora.h"
#include "wl_platform.h"
#include "wl_protocol.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal state variables                                           */
/* ------------------------------------------------------------------ */

static WL_App_State s_state = WL_APP_STATE_IDLE;

/** Millisecond timestamp when the race starts. */
static uint32_t s_race_start_ms = 0;

/** Whether the race has started. */
static bool s_race_started = false;

/** Timestamp of the last LoRa transmission (for throttling). */
static uint32_t s_last_tx_ms = 0;

/** Whether at least one checkpoint message has been sent successfully. */
static bool s_has_tx = false;

/** Timestamp of the most recent status report. */
static uint32_t s_last_status_report_ms = 0;

/** Whether the power-on timed checkpoint has been sent. */
static bool s_timed_checkpoint_sent = false;

/** Status text of the most recent operation. */
static char s_status_text[64] = "IDLE";
static WL_App_Diag s_diag;

/* ------------------------------------------------------------------ */
/*  Internal helper functions                                          */
/* ------------------------------------------------------------------ */

/** Send a checkpoint message (internal call with throttling logic). */
static void _send_checkpoint(uint32_t checkpoint_id)
{
    uint32_t now = WL_Platform_GetMillis();

    /* Throttle: first packet is not throttled; subsequent transmissions must
     * be spaced by at least WL_TX_MIN_INTERVAL_MS */
    if (s_has_tx &&
        ((uint32_t)(now - s_last_tx_ms) < g_wl_config.tx_min_interval_ms)) {
        s_diag.checkpoint_throttled_count += 1U;
        s_diag.last_checkpoint_id = checkpoint_id;
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u throttled", (unsigned)checkpoint_id);
        WL_Platform_DebugPrint("[App] ");
        WL_Platform_DebugPrint(s_status_text);
        WL_Platform_DebugPrint("\r\n");
        return;
    }

    /* Calculate elapsed time */
    uint32_t elapsed = 0;
    if (s_race_started) {
        elapsed = (uint32_t)(now - s_race_start_ms);
    }

    /* Build message */
    char msg[WL_MSG_MAX_LEN];
    uint16_t len = WL_Protocol_BuildCheckpointMsg(msg,
                                                   (uint16_t)sizeof(msg),
                                                   checkpoint_id,
                                                   elapsed);
    if (len == 0U) {
        snprintf(s_status_text, sizeof(s_status_text), "build message failed");
        WL_Platform_DebugPrint("[App] build checkpoint message failed\r\n");
        return;
    }

    /* Transmit via LoRa */
    WL_LoRa_Status st = WL_LoRa_EnqueueString(msg);

    s_diag.last_checkpoint_id = checkpoint_id;
    if (st == WL_LORA_OK) {
        uint32_t total_seconds = elapsed / 1000U;
        s_diag.checkpoint_enqueued_count += 1U;
        s_last_tx_ms = now;
        s_has_tx = true;
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u sent (%02u:%02u)",
                 (unsigned)checkpoint_id,
                 (unsigned)(total_seconds / 60U),
                 (unsigned)(total_seconds % 60U));
    } else {
        s_diag.checkpoint_enqueue_fail_count += 1U;
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u send failed(err=%d)",
                 (unsigned)checkpoint_id,
                 (int)st);
    }

    WL_Platform_DebugPrint("[App] ");
    WL_Platform_DebugPrint(s_status_text);
    WL_Platform_DebugPrint("\r\n");
}

static void _send_timed_checkpoint(void)
{
    if (s_timed_checkpoint_sent || g_wl_config.timed_checkpoint_delay_ms == 0U) {
        return;
    }

    if (WL_App_GetElapsedMs() < g_wl_config.timed_checkpoint_delay_ms) {
        return;
    }

    _send_checkpoint(g_wl_config.timed_checkpoint_id);
    s_timed_checkpoint_sent = true;
}

static void _send_periodic_status(void)
{
    uint32_t now = WL_Platform_GetMillis();
    char payload[80];
    char msg[WL_MSG_MAX_LEN];
    uint16_t len;
    const WL_LoRa_LinkStatus *link = WL_LoRa_GetLinkStatus();

    if (g_wl_config.status_report_period_ms == 0U) {
        return;
    }

    if ((uint32_t)(now - s_last_status_report_ms) < g_wl_config.status_report_period_ms) {
        return;
    }

    s_last_status_report_ms = now;

    snprintf(payload,
             sizeof(payload),
             "STATE=%d,Q=%u,S=%u,F=%u,R=%u",
             (int)s_state,
             (unsigned)link->queue_depth,
             (unsigned)link->tx_success_count,
             (unsigned)link->tx_fail_count,
             (unsigned)link->retry_count);

    len = WL_Protocol_BuildCustomMsg(msg, (uint16_t)sizeof(msg), payload);
    if (len == 0U) {
        return;
    }

    (void)WL_LoRa_Enqueue((const uint8_t *)msg, len);
}

/* ------------------------------------------------------------------ */
/*  Public interface implementation                                    */
/* ------------------------------------------------------------------ */

bool WL_App_Init(void)
{
    /* Initialize platform layer */
    WL_Platform_Init();

    s_state = WL_APP_STATE_IDLE;
    s_race_start_ms = 0;
    s_race_started = false;
    s_last_tx_ms = 0;
    s_has_tx = false;
    s_last_status_report_ms = 0;
    s_timed_checkpoint_sent = false;
    memset(&s_diag, 0, sizeof(s_diag));
    snprintf(s_status_text, sizeof(s_status_text), "init");

    /* Initialize LoRa module */
    WL_LoRa_Status st = WL_LoRa_Init();
    if (st != WL_LORA_OK) {
        s_state = WL_APP_STATE_ERROR;
        snprintf(s_status_text, sizeof(s_status_text),
                 "LoRa init failed(err=%d)",
                 (int)st);
        WL_Platform_DebugPrint("[App] ");
        WL_Platform_DebugPrint(s_status_text);
        WL_Platform_DebugPrint("\r\n");
        return false;
    }

    WL_LoRa_ServiceInit();
    WL_LoRa_SetAckEnabled(g_wl_config.ack_enable);

    s_state = WL_APP_STATE_READY;
    snprintf(s_status_text, sizeof(s_status_text), "ready");
    WL_Platform_DebugPrint("[App] init done, ready\r\n");
    return true;
}

void WL_App_Tick(void)
{
    if (s_state == WL_APP_STATE_RUNNING) {
        _send_timed_checkpoint();
        _send_periodic_status();
    }

    WL_LoRa_Tick();
}

void WL_App_StartRace(void)
{
    if (s_state != WL_APP_STATE_READY && s_state != WL_APP_STATE_FINISHED) {
        return;
    }

    s_race_start_ms = WL_Platform_GetMillis();
    s_race_started = true;
    s_last_tx_ms = 0;
    s_has_tx = false;
    s_last_status_report_ms = s_race_start_ms;
    s_timed_checkpoint_sent = false;
    s_state = WL_APP_STATE_RUNNING;
    snprintf(s_status_text, sizeof(s_status_text), "race started");
    WL_Platform_DebugPrint("[App] race started\r\n");
}

void WL_App_StopRace(void)
{
    if (s_state != WL_APP_STATE_RUNNING) {
        return;
    }

    s_state = WL_APP_STATE_FINISHED;
    uint32_t elapsed = WL_App_GetElapsedMs();
    snprintf(s_status_text, sizeof(s_status_text),
             "race finished (%ums)",
             (unsigned)elapsed);
    WL_Platform_DebugPrint("[App] ");
    WL_Platform_DebugPrint(s_status_text);
    WL_Platform_DebugPrint("\r\n");
}

uint32_t WL_App_GetElapsedMs(void)
{
    if (!s_race_started) {
        return 0;
    }

    return (uint32_t)(WL_Platform_GetMillis() - s_race_start_ms);
}

void WL_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    if (s_state != WL_APP_STATE_RUNNING) {
        WL_Platform_DebugPrint("[App] ignore checkpoint outside running\r\n");
        return;
    }

    _send_checkpoint(checkpoint_id);
}

WL_App_State WL_App_GetState(void)
{
    return s_state;
}

const char *WL_App_GetLastStatusText(void)
{
    return s_status_text;
}

const WL_App_Diag *WL_App_GetDiag(void)
{
    return &s_diag;
}
