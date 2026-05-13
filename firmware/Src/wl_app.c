/**
 * @file    wl_app.c
 * @brief   无线通信应用层实现 — 状态机、计时、检查点报文发送。
 */

#include "wl_app.h"
#include "wl_config.h"
#include "wl_lora.h"
#include "wl_platform.h"
#include "wl_protocol.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  内部状态变量                                                       */
/* ------------------------------------------------------------------ */

static WL_App_State s_state = WL_APP_STATE_IDLE;

/** 比赛开始时的毫秒时间戳。 */
static uint32_t s_race_start_ms = 0;

/** 比赛是否已启动。 */
static bool s_race_started = false;

/** 上一次 LoRa 发射的时间戳（用于节流控制）。 */
static uint32_t s_last_tx_ms = 0;

/** 是否已经成功发送过至少一条检查点报文。 */
static bool s_has_tx = false;

/** 最近一次状态上报时间戳。 */
static uint32_t s_last_status_report_ms = 0;

/** 最近一次操作的状态文本。 */
static char s_status_text[64] = "IDLE";

/* ------------------------------------------------------------------ */
/*  内部辅助函数                                                       */
/* ------------------------------------------------------------------ */

/** 发送检查点报文（内部调用，包含节流逻辑）。 */
static void _send_checkpoint(uint32_t checkpoint_id)
{
    uint32_t now = WL_Platform_GetMillis();

    /* 节流：首包不节流；后续两次发射间隔不小于 WL_TX_MIN_INTERVAL_MS */
    if (s_has_tx &&
        ((uint32_t)(now - s_last_tx_ms) < g_wl_config.tx_min_interval_ms)) {
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u throttled", (unsigned)checkpoint_id);
        WL_Platform_DebugPrint("[App] ");
        WL_Platform_DebugPrint(s_status_text);
        WL_Platform_DebugPrint("\r\n");
        return;
    }

    /* 计算已过时间 */
    uint32_t elapsed = 0;
    if (s_race_started) {
        elapsed = (uint32_t)(now - s_race_start_ms);
    }

    /* 构造报文 */
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

    /* 通过 LoRa 发射 */
    WL_LoRa_Status st = WL_LoRa_EnqueueString(msg);

    if (st == WL_LORA_OK) {
        s_last_tx_ms = now;
        s_has_tx = true;
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u sent (%ums)",
                 (unsigned)checkpoint_id,
                 (unsigned)elapsed);
    } else {
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u send failed(err=%d)",
                 (unsigned)checkpoint_id,
                 (int)st);
    }

    WL_Platform_DebugPrint("[App] ");
    WL_Platform_DebugPrint(s_status_text);
    WL_Platform_DebugPrint("\r\n");
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
/*  公共接口实现                                                       */
/* ------------------------------------------------------------------ */

bool WL_App_Init(void)
{
    /* 初始化平台层 */
    WL_Platform_Init();

    s_state = WL_APP_STATE_IDLE;
    s_race_start_ms = 0;
    s_race_started = false;
    s_last_tx_ms = 0;
    s_has_tx = false;
    s_last_status_report_ms = 0;
    snprintf(s_status_text, sizeof(s_status_text), "init");

    /* 初始化 LoRa 模块 */
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
    WL_Platform_DebugPrint("[App] init done\r\n");
    return true;
}

void WL_App_Tick(void)
{
    WL_LoRa_Tick();

    if (s_state == WL_APP_STATE_RUNNING) {
        _send_periodic_status();
    }
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
    s_last_status_report_ms = WL_Platform_GetMillis();
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
