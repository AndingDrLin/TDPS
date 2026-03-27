/**
 * @file    wl_config.c
 * @brief   无线模块默认运行时配置实例。
 */

#include "wl_config.h"

const WL_Config g_wl_config = {
    /* 队伍信息 */
    .team_number            = WL_TEAM_NUMBER,
    .team_name              = WL_TEAM_NAME,

    /* LoRa 参数 */
    .lora_addr              = WL_LORA_ADDR,
    .lora_channel           = WL_LORA_CHANNEL,
    .lora_net_id            = WL_LORA_NET_ID,
    .lora_air_rate          = WL_LORA_AIR_RATE,
    .lora_tx_power          = WL_LORA_TX_POWER,
    .lora_trans_mode        = WL_LORA_TRANS_MODE,
    .lora_packet_size       = WL_LORA_PACKET_SIZE,

    /* UART 参数 */
    .uart_baudrate          = WL_UART_BAUDRATE,

    /* 超时参数 */
    .at_response_timeout_ms = WL_AT_RESPONSE_TIMEOUT_MS,
    .mode_switch_delay_ms   = WL_MODE_SWITCH_DELAY_MS,
    .power_on_delay_ms      = WL_POWER_ON_DELAY_MS,
    .tx_min_interval_ms     = WL_TX_MIN_INTERVAL_MS,

    /* 检查点 */
    .checkpoint_arch_2_1    = WL_CHECKPOINT_ARCH_2_1,
    .checkpoint_arch_2_2    = WL_CHECKPOINT_ARCH_2_2,
};
