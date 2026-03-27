/**
 * @file    wl_app.c
 * @brief   无线通信应用层实现 — 状态机、计时、检查点报文发送。
 */

#include "wl_app.h"
#include "wl_platform.h"
#include "wl_lora.h"
#include "wl_protocol.h"
#include "wl_config.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  内部状态变量                                                       */
/* ------------------------------------------------------------------ */

static WL_App_State s_state = WL_APP_STATE_IDLE;

/** 比赛开始时的毫秒时间戳（0 表示尚未开始）。 */
static uint32_t s_race_start_ms = 0;

/** 上一次 LoRa 发射的时间戳（用于节流控制）。 */
static uint32_t s_last_tx_ms = 0;

/** 最近一次操作的状态文本。 */
static char s_status_text[64] = "空闲";

/* ------------------------------------------------------------------ */
/*  内部辅助函数                                                       */
/* ------------------------------------------------------------------ */

/** 发送检查点报文（内部调用，包含节流逻辑）。 */
static void _send_checkpoint(uint32_t checkpoint_id)
{
    uint32_t now = WL_Platform_GetMillis();

    /* 节流：两次发射间隔不小于 WL_TX_MIN_INTERVAL_MS */
    if ((now - s_last_tx_ms) < g_wl_config.tx_min_interval_ms) {
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u 节流中，跳过", (unsigned)checkpoint_id);
        WL_Platform_DebugPrint("[App] ");
        WL_Platform_DebugPrint(s_status_text);
        WL_Platform_DebugPrint("\r\n");
        return;
    }

    /* 计算已过时间 */
    uint32_t elapsed = 0;
    if (s_race_start_ms > 0) {
        elapsed = now - s_race_start_ms;
    }

    /* 构造报文 */
    char msg[WL_MSG_MAX_LEN];
    uint16_t len = WL_Protocol_BuildCheckpointMsg(msg, sizeof(msg),
                                                   checkpoint_id,
                                                   elapsed);
    if (len == 0) {
        snprintf(s_status_text, sizeof(s_status_text), "报文构造失败");
        return;
    }

    /* 通过 LoRa 发射 */
    WL_LoRa_Status st = WL_LoRa_SendString(msg);

    if (st == WL_LORA_OK) {
        s_last_tx_ms = now;
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u 已发送 (%ums)",
                 (unsigned)checkpoint_id, (unsigned)elapsed);
    } else {
        snprintf(s_status_text, sizeof(s_status_text),
                 "CP%u 发送失败(err=%d)",
                 (unsigned)checkpoint_id, (int)st);
    }

    WL_Platform_DebugPrint("[App] ");
    WL_Platform_DebugPrint(s_status_text);
    WL_Platform_DebugPrint("\r\n");
}

/* ------------------------------------------------------------------ */
/*  公共接口实现                                                       */
/* ------------------------------------------------------------------ */

bool WL_App_Init(void)
{
    /* 初始化平台层 */
    WL_Platform_Init();

    /* 初始化 LoRa 模块 */
    WL_LoRa_Status st = WL_LoRa_Init();
    if (st != WL_LORA_OK) {
        s_state = WL_APP_STATE_ERROR;
        snprintf(s_status_text, sizeof(s_status_text),
                 "LoRa 初始化失败(err=%d)", (int)st);
        WL_Platform_DebugPrint("[App] ");
        WL_Platform_DebugPrint(s_status_text);
        WL_Platform_DebugPrint("\r\n");
        return false;
    }

    s_state = WL_APP_STATE_READY;
    snprintf(s_status_text, sizeof(s_status_text), "就绪，等待比赛开始");
    WL_Platform_DebugPrint("[App] 初始化完成\r\n");
    return true;
}

void WL_App_Tick(void)
{
    /* 目前 tick 函数预留用于：
     * - 周期性心跳报文（如需要）
     * - 检查模块状态
     * - 处理接收数据
     * 当前版本为事件驱动（由 NotifyCheckpoint 触发），此处暂无额外逻辑。
     */
    (void)0;
}

void WL_App_StartRace(void)
{
    s_race_start_ms = WL_Platform_GetMillis();
    s_state = WL_APP_STATE_RUNNING;
    snprintf(s_status_text, sizeof(s_status_text), "比赛开始");
    WL_Platform_DebugPrint("[App] 比赛计时开始\r\n");
}

void WL_App_StopRace(void)
{
    s_state = WL_APP_STATE_FINISHED;
    uint32_t elapsed = WL_App_GetElapsedMs();
    snprintf(s_status_text, sizeof(s_status_text),
             "比赛结束 (总计 %ums)", (unsigned)elapsed);
    WL_Platform_DebugPrint("[App] ");
    WL_Platform_DebugPrint(s_status_text);
    WL_Platform_DebugPrint("\r\n");
}

uint32_t WL_App_GetElapsedMs(void)
{
    if (s_race_start_ms == 0) {
        return 0;
    }
    return WL_Platform_GetMillis() - s_race_start_ms;
}

void WL_App_NotifyCheckpoint(uint32_t checkpoint_id)
{
    if (s_state != WL_APP_STATE_RUNNING && s_state != WL_APP_STATE_READY) {
        WL_Platform_DebugPrint("[App] 非比赛状态，忽略检查点通知\r\n");
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
