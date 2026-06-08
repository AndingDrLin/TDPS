/**
 * @file    wl_config.h
 * @brief   Wireless communication module -- global configuration.
 *
 * All tunable parameters for the LoRa wireless subsystem are
 * consolidated in this file; changing parameters does not require
 * modifications to the driver or application layer code.
 *
 * Hardware: EWM22A-900BWL22S (LoRa 850-930 MHz)
 * MCU:      STM32F407VET6
 */

#ifndef WL_CONFIG_H
#define WL_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Team information                                                    */
/* ------------------------------------------------------------------ */
#define WL_TEAM_NUMBER          15
#define WL_TEAM_NAME            "PTSD"

/* ------------------------------------------------------------------ */
/*  LoRa RF parameters (must match the gateway receiver)                */
/* ------------------------------------------------------------------ */

/** Module address. */
#define WL_LORA_ADDR            0x0001

/** Channel index. 900-series actual frequency = 850.125 + CH x 1 MHz; CH=10 -> 860.125 MHz. */
#define WL_LORA_CHANNEL         10

/** Network ID (0-255). Must be the same on both communicating parties. */
#define WL_LORA_NET_ID          0x00

/** Air data rate index (0-7). 0/1/2 = 2.4 kbps. */
#define WL_LORA_AIR_RATE        0

/** Transmit power index. 0 = 22 dBm (maximum). */
#define WL_LORA_TX_POWER        0

/** Transmission mode. 0 = transparent, 1 = fixed-point. */
#define WL_LORA_TRANS_MODE      1

/** Packet size index. 0 = 240 bytes. */
#define WL_LORA_PACKET_SIZE     0

/** LoRa encryption key. 0 = no encryption. */
#define WL_LORA_KEY             0

/** Fixed-point transmission destination address. */
#define WL_LORA_FIXED_DEST_ADDR     0x0001
#define WL_LORA_FIXED_DEST_CHANNEL  WL_LORA_CHANNEL

/* ------------------------------------------------------------------ */
/*  UART parameters (serial communication between module and MCU)       */
/* ------------------------------------------------------------------ */

/** Baud rate for communication with the EWM22A module. */
#define WL_UART_BAUDRATE        115200U

/* UART data format is fixed at 8N1 (8 data bits, no parity, 1 stop bit). */

/* ------------------------------------------------------------------ */
/*  Timeout and delay parameters                                       */
/* ------------------------------------------------------------------ */

/** Timeout waiting for AT command response (milliseconds). */
#define WL_AT_RESPONSE_TIMEOUT_MS   2000U

/** Delay after sending AT+HMODE to switch mode and wait for module reboot (ms). */
#define WL_MODE_SWITCH_DELAY_MS     1000U

/** Delay after power-on before sending the first AT command (ms). */
#define WL_POWER_ON_DELAY_MS        500U

/** Minimum interval between LoRa transmissions (ms), to avoid channel congestion. */
#define WL_TX_MIN_INTERVAL_MS       500U

/** Send transaction timeout (ms). Keep short to avoid blocking the control loop. */
#define WL_TX_TIMEOUT_MS            50U

/** ACK wait timeout (ms). */
#define WL_ACK_TIMEOUT_MS           300U

/** Maximum number of send retries on failure. */
#define WL_TX_RETRY_MAX             2U

/** ACK verification toggle (disabled by default, enable after gateway interop testing). */
#define WL_ACK_ENABLE               false

/** Send AT configuration at startup to ensure module parameters match firmware. */
#define WL_LORA_RUN_AT_INIT         false

/** Status report period (ms). 0 = disabled. */
#define WL_STATUS_REPORT_PERIOD_MS  0U

/** Delay before auto-sending a checkpoint message after power-on (ms); 0 = disabled. */
#define WL_TIMED_CHECKPOINT_DELAY_MS 15000U

/* ------------------------------------------------------------------ */
/*  Checkpoint IDs (defined by the line-following system, triggered     */
/*  when passing through an arch)                                      */
/* ------------------------------------------------------------------ */

/** Checkpoint ID for arch 2.1. */
#define WL_CHECKPOINT_ARCH_2_1      21U

/** Checkpoint ID for arch 2.2. */
#define WL_CHECKPOINT_ARCH_2_2      22U

/** Checkpoint ID used for the timed auto-send on power-up. */
#define WL_TIMED_CHECKPOINT_ID      WL_CHECKPOINT_ARCH_2_1

/* ------------------------------------------------------------------ */
/*  Buffer sizes                                                       */
/* ------------------------------------------------------------------ */

/** Maximum payload length per LoRa transmission (bytes). */
#define WL_TX_PAYLOAD_MAX           128U

/** UART receive ring buffer size (bytes). */
#define WL_UART_RX_BUF_SIZE        256U

/** UART transmit buffer size (bytes). */
#define WL_UART_TX_BUF_SIZE        256U

/** Asynchronous send queue depth. */
#define WL_TX_QUEUE_SIZE            16U

/* ------------------------------------------------------------------ */
/*  Runtime configuration structure                                    */
/*  Mirrors the macros above so parameters can be changed at runtime   */
/*  without recompilation.                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Team information */
    uint8_t     team_number;
    const char *team_name;

    /* LoRa parameters */
    uint16_t    lora_addr;
    uint8_t     lora_channel;
    uint8_t     lora_net_id;
    uint8_t     lora_air_rate;
    uint8_t     lora_tx_power;
    uint8_t     lora_trans_mode;
    uint8_t     lora_packet_size;
    uint16_t    lora_key;
    uint16_t    lora_fixed_dest_addr;
    uint8_t     lora_fixed_dest_channel;

    /* UART parameters */
    uint32_t    uart_baudrate;

    /* Timeout parameters */
    uint32_t    at_response_timeout_ms;
    uint32_t    mode_switch_delay_ms;
    uint32_t    power_on_delay_ms;
    uint32_t    tx_min_interval_ms;
    uint32_t    tx_timeout_ms;
    uint32_t    ack_timeout_ms;
    uint8_t     tx_retry_max;
    bool        ack_enable;
    bool        lora_run_at_init;
    uint32_t    status_report_period_ms;
    uint32_t    timed_checkpoint_delay_ms;

    /* Checkpoint parameters */
    uint32_t    checkpoint_arch_2_1;
    uint32_t    checkpoint_arch_2_2;
    uint32_t    timed_checkpoint_id;

    /* Queue parameters */
    uint16_t    tx_queue_size;
} WL_Config;

/** Global configuration instance (defined in wl_config.c). */
extern const WL_Config g_wl_config;

#endif /* WL_CONFIG_H */
