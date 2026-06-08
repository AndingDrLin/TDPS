/**
 * @file    wl_lora.h
 * @brief   EWM22A-900BWL22S LoRa driver layer interface.
 *
 * Wraps AT command interaction, module initialization, mode switching,
 * and data transmission. The upper layer only needs to call the
 * concise API to perform LoRa communication.
 */

#ifndef WL_LORA_H
#define WL_LORA_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Return value definitions                                           */
/* ------------------------------------------------------------------ */

typedef enum {
    WL_LORA_OK = 0,         /**< Operation succeeded */
    WL_LORA_ERR_TIMEOUT,    /**< Response wait timed out */
    WL_LORA_ERR_RESPONSE,   /**< Received an unexpected response */
    WL_LORA_ERR_BUSY,       /**< Module busy (AUX is low) */
    WL_LORA_ERR_PARAM,      /**< Parameter error */
} WL_LoRa_Status;

/** Asynchronous link status and diagnostic counters. */
typedef struct {
    uint16_t queue_depth;          /**< Current send queue depth */
    uint16_t queue_dropped;        /**< Number of items dropped due to queue full */
    uint16_t retry_count;          /**< Number of retries performed */
    uint16_t tx_success_count;     /**< Number of successful transmissions */
    uint16_t tx_fail_count;        /**< Number of failed transmissions */
    bool waiting_ack;              /**< Whether currently waiting for an ACK */
    bool ack_enabled;              /**< Whether ACK mode is enabled */
    WL_LoRa_Status last_error;     /**< Last error code */
} WL_LoRa_LinkStatus;

/* ------------------------------------------------------------------ */
/*  Initialization and configuration                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize the LoRa module.
 *
 * Procedure: wait for AUX ready -> enter configuration mode -> set
 * address/channel/Network ID/air data rate/tx power/transmission mode/
 * key -> switch to UART/LoRa working mode.
 *
 * @return WL_LORA_OK on success, or an error code.
 */
WL_LoRa_Status WL_LoRa_Init(void);

/**
 * @brief Wait for the AUX pin to go high (module ready).
 *
 * @param timeout_ms Maximum wait time in milliseconds.
 * @return true = ready, false = timed out.
 */
bool WL_LoRa_WaitAUX(uint32_t timeout_ms);

/* ------------------------------------------------------------------ */
/*  AT command interaction                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Send an AT command and wait for the response.
 *
 * @param cmd          AT command string (without newline, e.g. "AT+ADDR=0").
 * @param resp_buf     Buffer to store the response (may be NULL if the
 *                     response content is not needed).
 * @param resp_buf_len Length of the response buffer.
 * @param timeout_ms   Response wait timeout in milliseconds.
 * @return WL_LORA_OK if "AT_OK" is received, otherwise an error code.
 */
WL_LoRa_Status WL_LoRa_SendAT(const char *cmd,
                                char *resp_buf,
                                uint16_t resp_buf_len,
                                uint32_t timeout_ms);

/* ------------------------------------------------------------------ */
/*  Data transmission                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Send data via LoRa working mode.
 *
 * In Fixed Mode the driver automatically prepends the destination
 * address and channel header.
 *
 * @param data  Pointer to the data.
 * @param len   Data length in bytes, must not exceed WL_TX_PAYLOAD_MAX.
 * @return WL_LORA_OK on success, or an error code.
 */
WL_LoRa_Status WL_LoRa_Send(const uint8_t *data, uint16_t len);

/**
 * @brief Send a null-terminated string via LoRa working mode.
 *
 * @param str  The string to send.
 * @return WL_LORA_OK on success, or an error code.
 */
WL_LoRa_Status WL_LoRa_SendString(const char *str);

/* ------------------------------------------------------------------ */
/*  Asynchronous send service (non-blocking main loop)                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize the async send service state.
 *
 * Should be called after WL_LoRa_Init() succeeds.
 */
void WL_LoRa_ServiceInit(void);

/**
 * @brief Enqueue data for transmission; actual send is driven by WL_LoRa_Tick().
 * @param data  Pointer to the data.
 * @param len   Data length in bytes.
 * @return WL_LORA_OK on success, or an error code.
 */
WL_LoRa_Status WL_LoRa_Enqueue(const uint8_t *data, uint16_t len);

/**
 * @brief Enqueue a string for transmission (excluding null terminator).
 * @param str  The string to send.
 * @return WL_LORA_OK on success, or an error code.
 */
WL_LoRa_Status WL_LoRa_EnqueueString(const char *str);

/**
 * @brief Non-blocking tick driving transmission, timeout/retry, and optional ACK.
 *
 * Call periodically from the main while(1) loop.
 */
void WL_LoRa_Tick(void);

/**
 * @brief Enable or disable the ACK wait logic at runtime.
 * @param enable true to enable, false to disable.
 */
void WL_LoRa_SetAckEnabled(bool enable);

/**
 * @brief Query the async service status.
 * @return Pointer to the link status (read-only).
 */
const WL_LoRa_LinkStatus *WL_LoRa_GetLinkStatus(void);

/* ------------------------------------------------------------------ */
/*  Status queries                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Check if the module is in a ready state (AUX is high).
 * @return true if ready, false otherwise.
 */
bool WL_LoRa_IsReady(void);

#endif /* WL_LORA_H */
