/**
 * @file    wl_config.h
 * @brief   无线通信模块 — 全局配置文件
 *
 * LoRa 无线子系统的所有可调参数集中在此文件中，
 * 修改参数时无需改动驱动层或应用层代码。
 *
 * 硬件：EWM22A-900BWL22S（LoRa 850–930 MHz）
 * MCU： STM32F407VET6
 */

#ifndef WL_CONFIG_H
#define WL_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  队伍信息                                                           */
/* ------------------------------------------------------------------ */
#define WL_TEAM_NUMBER          15
#define WL_TEAM_NAME            "TDPS"

/* ------------------------------------------------------------------ */
/*  LoRa 射频参数（必须与拱门接收端一致）                                */
/* ------------------------------------------------------------------ */

/** 模块地址（0x0000 = 默认 / 可广播）。 */
#define WL_LORA_ADDR            0x0000

/** 信道索引。实际频率 = 850.125 + CH × 1 MHz。
 *  默认 0x12（18）→ 868.125 MHz。 */
#define WL_LORA_CHANNEL         18

/** 网络 ID（0–255）。通信双方必须一致。 */
#define WL_LORA_NET_ID          0

/** 空中速率索引（0–7）。0/1/2 = 2.4 kbps（默认）。 */
#define WL_LORA_AIR_RATE        2

/** 发射功率索引。0 = 22 dBm（最大功率）。 */
#define WL_LORA_TX_POWER        0

/** 传输模式。0 = 透明传输，1 = 定点传输。 */
#define WL_LORA_TRANS_MODE      0

/** 分包长度索引。0 = 240 字节（默认）。 */
#define WL_LORA_PACKET_SIZE     0

/* ------------------------------------------------------------------ */
/*  UART 参数（模块与 MCU 之间的串口通信）                               */
/* ------------------------------------------------------------------ */

/** 与 EWM22A 模块通信使用的波特率。 */
#define WL_UART_BAUDRATE        115200U

/** UART 数据格式固定为 8N1（8位数据位、无校验、1位停止位）。 */

/* ------------------------------------------------------------------ */
/*  超时与延时参数                                                      */
/* ------------------------------------------------------------------ */

/** 等待 AT 指令响应的超时时间（毫秒）。 */
#define WL_AT_RESPONSE_TIMEOUT_MS   2000U

/** 发送 AT+HMODE 切换模式后等待模块重启的延时（毫秒）。 */
#define WL_MODE_SWITCH_DELAY_MS     1000U

/** 模块上电后、发送第一条 AT 指令前的等待延时（毫秒）。 */
#define WL_POWER_ON_DELAY_MS        500U

/** 两次 LoRa 发射之间的最小间隔（毫秒），避免信道拥堵。 */
#define WL_TX_MIN_INTERVAL_MS       500U

/** 发送事务超时（毫秒）。 */
#define WL_TX_TIMEOUT_MS            800U

/** ACK 等待超时（毫秒）。 */
#define WL_ACK_TIMEOUT_MS           300U

/** 发送失败重试次数上限。 */
#define WL_TX_RETRY_MAX             2U

/** ACK 校验开关（默认关闭，待网关联调后启用）。 */
#define WL_ACK_ENABLE               false

/** 状态上报节拍（毫秒）。 */
#define WL_STATUS_REPORT_PERIOD_MS  1000U

/* ------------------------------------------------------------------ */
/*  检查点 ID（由巡线系统定义，经过拱门时触发无线发射）                    */
/* ------------------------------------------------------------------ */

/** 拱门 2.1 对应的检查点 ID。 */
#define WL_CHECKPOINT_ARCH_2_1      21U

/** 拱门 2.2 对应的检查点 ID。 */
#define WL_CHECKPOINT_ARCH_2_2      22U

/* ------------------------------------------------------------------ */
/*  缓冲区大小                                                         */
/* ------------------------------------------------------------------ */

/** 单次 LoRa 发射载荷的最大长度（字节）。 */
#define WL_TX_PAYLOAD_MAX           128U

/** UART 接收环形缓冲区大小（字节）。 */
#define WL_UART_RX_BUF_SIZE        256U

/** UART 发送缓冲区大小（字节）。 */
#define WL_UART_TX_BUF_SIZE        256U

/** 异步发送队列深度。 */
#define WL_TX_QUEUE_SIZE            16U

/* ------------------------------------------------------------------ */
/*  运行时配置结构体                                                    */
/*  镜像上面的宏定义，使得参数可以在运行时修改而无需重新编译。             */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 队伍信息 */
    uint8_t     team_number;
    const char *team_name;

    /* LoRa 参数 */
    uint16_t    lora_addr;
    uint8_t     lora_channel;
    uint8_t     lora_net_id;
    uint8_t     lora_air_rate;
    uint8_t     lora_tx_power;
    uint8_t     lora_trans_mode;
    uint8_t     lora_packet_size;

    /* UART 参数 */
    uint32_t    uart_baudrate;

    /* 超时参数 */
    uint32_t    at_response_timeout_ms;
    uint32_t    mode_switch_delay_ms;
    uint32_t    power_on_delay_ms;
    uint32_t    tx_min_interval_ms;
    uint32_t    tx_timeout_ms;
    uint32_t    ack_timeout_ms;
    uint8_t     tx_retry_max;
    bool        ack_enable;
    uint32_t    status_report_period_ms;

    /* 检查点 */
    uint32_t    checkpoint_arch_2_1;
    uint32_t    checkpoint_arch_2_2;

    /* 队列参数 */
    uint16_t    tx_queue_size;
} WL_Config;

/** 全局配置实例（定义于 wl_config.c）。 */
extern const WL_Config g_wl_config;

#endif /* WL_CONFIG_H */
