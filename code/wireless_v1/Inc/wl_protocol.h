/**
 * @file    wl_protocol.h
 * @brief   无线通信数据协议层 — 拱门报文打包与解析。
 *
 * 定义小车经过拱门时需要发送的报文格式，
 * 以及构造报文的 API。
 *
 * 报文格式（纯 ASCII，便于拱门端解析）：
 *   "TEAM=<队号>,NAME=<队名>,CP=<检查点ID>,TIME=<毫秒时间戳>\n"
 */

#ifndef WL_PROTOCOL_H
#define WL_PROTOCOL_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  报文最大长度                                                       */
/* ------------------------------------------------------------------ */

/** 单条报文的最大字符数（含结尾 '\0'）。 */
#define WL_MSG_MAX_LEN      128

/* ------------------------------------------------------------------ */
/*  报文构造                                                           */
/* ------------------------------------------------------------------ */

/**
 * 构造一条拱门检查点报文。
 *
 * @param buf           输出缓冲区，长度至少 WL_MSG_MAX_LEN。
 * @param buf_len       缓冲区长度。
 * @param checkpoint_id 检查点 ID（例如 21 = 拱门 2.1）。
 * @param elapsed_ms    从比赛开始到当前的毫秒数。
 * @return              写入缓冲区的字符数（不含 '\0'），失败返回 0。
 */
uint16_t WL_Protocol_BuildCheckpointMsg(char *buf,
                                         uint16_t buf_len,
                                         uint32_t checkpoint_id,
                                         uint32_t elapsed_ms);

/**
 * 构造一条自定义文本报文（用于调试或扩展）。
 *
 * @param buf       输出缓冲区。
 * @param buf_len   缓冲区长度。
 * @param text      自定义文本内容。
 * @return          写入缓冲区的字符数（不含 '\0'），失败返回 0。
 */
uint16_t WL_Protocol_BuildCustomMsg(char *buf,
                                     uint16_t buf_len,
                                     const char *text);

#endif /* WL_PROTOCOL_H */
