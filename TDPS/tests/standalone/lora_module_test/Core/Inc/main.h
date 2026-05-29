#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#define LORA_LINK_Pin GPIO_PIN_0
#define LORA_LINK_GPIO_Port GPIOD
#define LORA_WAKE_Pin GPIO_PIN_1
#define LORA_WAKE_GPIO_Port GPIOD
#define LORA_AUX_Pin GPIO_PIN_0
#define LORA_AUX_GPIO_Port GPIOE
#define LORA_RST_Pin GPIO_PIN_1
#define LORA_RST_GPIO_Port GPIOE

#define LORA_TEST_CMD_NONE 0U
#define LORA_TEST_CMD_STATUS 1U
#define LORA_TEST_CMD_RESET 2U
#define LORA_TEST_CMD_CONFIG 3U
#define LORA_TEST_CMD_SEND 4U
#define LORA_TEST_CMD_WAKE_LOW 5U
#define LORA_TEST_CMD_WAKE_HIGH 6U
#define LORA_TEST_CMD_AUTO_OFF 7U
#define LORA_TEST_CMD_AUTO_ON 8U

typedef struct {
    uint32_t magic;
    uint32_t uptime_ms;
    uint32_t command;
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
    uint32_t at_ok_count;
    uint32_t at_error_count;
    uint32_t at_step;
    int32_t last_status;
    uint8_t aux;
    uint8_t link;
    uint8_t wake;
    uint8_t rst;
    uint8_t auto_send;
    char last_command[32];
    char last_response[128];
    char last_payload[64];
    char last_rx[64];
} LoraTestState;

extern UART_HandleTypeDef huart5;
extern LoraTestState g_lora_test;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif
