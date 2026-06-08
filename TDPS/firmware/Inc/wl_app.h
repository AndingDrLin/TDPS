/**
 * @file    wl_app.h
 * @brief   Wireless communication application layer -- state machine and public API.
 *
 * Manages the complete flow of race timing, checkpoint triggering, and
 * message transmission. The line-following module triggers wireless
 * transmission by calling WL_App_NotifyCheckpoint().
 */

#ifndef WL_APP_H
#define WL_APP_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Application state definitions                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    WL_APP_STATE_IDLE = 0,      /**< Idle, waiting for initialization or race start */
    WL_APP_STATE_READY,         /**< LoRa initialized, waiting for race start */
    WL_APP_STATE_RUNNING,       /**< Race in progress, timer running */
    WL_APP_STATE_FINISHED,      /**< Race finished */
    WL_APP_STATE_ERROR,         /**< Error state */
} WL_App_State;

/** Diagnostic counters for checkpoint transmission. */
typedef struct {
    uint16_t checkpoint_enqueued_count;     /**< Number of checkpoints successfully enqueued */
    uint16_t checkpoint_enqueue_fail_count; /**< Number of checkpoint enqueue failures */
    uint16_t checkpoint_throttled_count;    /**< Number of checkpoints throttled (dropped) */
    uint32_t last_checkpoint_id;            /**< ID of the last checkpoint triggered */
} WL_App_Diag;

/* ------------------------------------------------------------------ */
/*  Initialization and main loop                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize the wireless communication application layer.
 *
 * Internally calls WL_Platform_Init() and WL_LoRa_Init().
 *
 * @return true on success, false on initialization failure.
 */
bool WL_App_Init(void);

/**
 * @brief Application layer main-loop tick.
 *
 * Call periodically from the main while(1) loop to process state
 * transitions and pending messages.
 */
void WL_App_Tick(void);

/* ------------------------------------------------------------------ */
/*  Race timing control                                                */
/* ------------------------------------------------------------------ */

/** @brief Start race timing. */
void WL_App_StartRace(void);

/** @brief Stop race timing. */
void WL_App_StopRace(void);

/** @brief Get the elapsed milliseconds since race start. Returns 0 if not started. */
uint32_t WL_App_GetElapsedMs(void);

/* ------------------------------------------------------------------ */
/*  Checkpoint notification                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Notify the application layer that the car has passed a checkpoint.
 *
 * Internally constructs a message and transmits it via LoRa.
 *
 * @param checkpoint_id Checkpoint ID (e.g. WL_CHECKPOINT_ARCH_2_1).
 */
void WL_App_NotifyCheckpoint(uint32_t checkpoint_id);

/* ------------------------------------------------------------------ */
/*  Status queries                                                     */
/* ------------------------------------------------------------------ */

/** @brief Get the current application state. */
WL_App_State WL_App_GetState(void);

/** @brief Get the last transmission status text (for debug/display). */
const char *WL_App_GetLastStatusText(void);

/** @brief Get checkpoint transmission diagnostic counters. */
const WL_App_Diag *WL_App_GetDiag(void);

#endif /* WL_APP_H */
