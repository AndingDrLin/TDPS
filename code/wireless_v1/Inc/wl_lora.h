/**
 * @file    wl_lora.h
 * @brief   EWM22A-900BWL22S LoRa 驱动层接口。
 *
 * 封装 AT 指令交互、模块初始化、模式切换和数据发送，
 * 上层只需调用简洁的 API 即可完成 LoRa 通信。
 */

#ifndef WL_LORA_H
#define WL_LORA_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  返回值定义                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    WL_LORA_OK = 0,         /**< 操作成功 */
    WL_LORA_ERR_TIMEOUT,    /**< 等待响应超时 */
    WL_LORA_ERR_RESPONSE,   /**< 收到非预期响应 */
    WL_LORA_ERR_BUSY,       /**< 模块忙碌（AUX 为低） */
    WL_LORA_ERR_PARAM,      /**< 参数错误 */
} WL_LoRa_Status;

/* ------------------------------------------------------------------ */
/*  初始化与配置                                                       */
/* ------------------------------------------------------------------ */

/**
 * 初始化 LoRa 模块。
 * 流程：等待 AUX 就绪 → 进入配置模式 → 设置地址/信道/网络ID/空中速率/
 *       发射功率/传输模式 → 切换到 UART/LoRa 透传模式。
 *
 * @return WL_LORA_OK 成功，其他值为错误码。
 */
WL_LoRa_Status WL_LoRa_Init(void);

/**
 * 等待 AUX 引脚变为高电平（模块就绪）。
 *
 * @param timeout_ms  最大等待时间（毫秒）。
 * @return true = 就绪，false = 超时。
 */
bool WL_LoRa_WaitAUX(uint32_t timeout_ms);

/* ------------------------------------------------------------------ */
/*  AT 指令交互                                                        */
/* ------------------------------------------------------------------ */

/**
 * 发送一条 AT 指令并等待响应。
 *
 * @param cmd          AT 指令字符串（不含换行符，例如 "AT+ADDR=0"）。
 * @param resp_buf     用于存放响应的缓冲区（可为 NULL 表示不关心响应内容）。
 * @param resp_buf_len 响应缓冲区长度。
 * @param timeout_ms   等待响应的超时时间（毫秒）。
 * @return WL_LORA_OK 收到 "AT_OK"，否则返回错误码。
 */
WL_LoRa_Status WL_LoRa_SendAT(const char *cmd,
                                char *resp_buf,
                                uint16_t resp_buf_len,
                                uint32_t timeout_ms);

/* ------------------------------------------------------------------ */
/*  数据发送                                                           */
/* ------------------------------------------------------------------ */

/**
 * 通过 LoRa 透传模式发送一段数据。
 * 在模式 1（UART/LoRa 透传）下，直接将数据写入 UART 即由模块发射。
 *
 * @param data  数据指针。
 * @param len   数据长度（字节），不超过 WL_TX_PAYLOAD_MAX。
 * @return WL_LORA_OK 成功，其他值为错误码。
 */
WL_LoRa_Status WL_LoRa_Send(const uint8_t *data, uint16_t len);

/**
 * 通过 LoRa 透传模式发送一个以 '\0' 结尾的字符串。
 *
 * @param str  要发送的字符串。
 * @return WL_LORA_OK 成功，其他值为错误码。
 */
WL_LoRa_Status WL_LoRa_SendString(const char *str);

/* ------------------------------------------------------------------ */
/*  状态查询                                                           */
/* ------------------------------------------------------------------ */

/**
 * 查询模块是否处于就绪状态（AUX 为高）。
 */
bool WL_LoRa_IsReady(void);

#endif /* WL_LORA_H */
