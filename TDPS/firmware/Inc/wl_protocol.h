/**
 * @file    wl_protocol.h
 * @brief   Wireless communication data protocol layer -- arch message
 *          packing and parsing.
 *
 * Defines the message format to be transmitted when the car passes
 * through an arch, and the API for constructing messages.
 *
 * Message format (pure ASCII, for easy parsing on the gateway side):
 *   "TEAM=<team_number>,NAME=<team_name>,CP=<checkpoint_id>,TIME=<MM>:<SS>\n"
 */

#ifndef WL_PROTOCOL_H
#define WL_PROTOCOL_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Message maximum length                                             */
/* ------------------------------------------------------------------ */

/** Maximum character count per message (including the trailing '\0'). */
#define WL_MSG_MAX_LEN      128

/* ------------------------------------------------------------------ */
/*  Message construction                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Construct an arch checkpoint message.
 *
 * @param[out] buf           Output buffer, at least WL_MSG_MAX_LEN bytes.
 * @param      buf_len       Buffer length in bytes.
 * @param      checkpoint_id Checkpoint ID (e.g. 21 = arch 2.1).
 * @param      elapsed_ms    Milliseconds since race start; converted to MM:SS in the message.
 * @return     Number of characters written to the buffer (excluding '\0'); 0 on failure.
 */
uint16_t WL_Protocol_BuildCheckpointMsg(char *buf,
                                         uint16_t buf_len,
                                         uint32_t checkpoint_id,
                                         uint32_t elapsed_ms);

/**
 * @brief Construct a custom text message (for debugging or extensions).
 *
 * @param[out] buf       Output buffer.
 * @param      buf_len   Buffer length in bytes.
 * @param      text      Custom text content.
 * @return     Number of characters written to the buffer (excluding '\0'); 0 on failure.
 */
uint16_t WL_Protocol_BuildCustomMsg(char *buf,
                                     uint16_t buf_len,
                                     const char *text);

#endif /* WL_PROTOCOL_H */
