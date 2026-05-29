#include "main.h"
#include <stdio.h>
#include <string.h>

#define LORA_TEST_MAGIC 0x4C4F5241UL
#define LORA_RX_BUF_SIZE 256U
#define LORA_RESPONSE_TIMEOUT_MS 2000U
#define LORA_CHANNEL 10U
#define LORA_ADDR 0x0001U
#define LORA_NET_ID 0U
#define LORA_KEY 0U
#define LORA_FIXED_DEST_ADDR 0xFFFFU
#define LORA_AUTO_SEND_PERIOD_MS 1000U

UART_HandleTypeDef huart5;
LoraTestState g_lora_test;

static volatile uint8_t s_rx_byte;
static volatile uint8_t s_rx_buf[LORA_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static uint32_t s_last_auto_send_ms;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART5_Init(void);
static void lora_uart_rx_start(void);

static void lora_update_pins(void)
{
    g_lora_test.aux = (uint8_t)HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin);
    g_lora_test.link = (uint8_t)HAL_GPIO_ReadPin(LORA_LINK_GPIO_Port, LORA_LINK_Pin);
    g_lora_test.wake = (uint8_t)HAL_GPIO_ReadPin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin);
    g_lora_test.rst = (uint8_t)HAL_GPIO_ReadPin(LORA_RST_GPIO_Port, LORA_RST_Pin);
}

static uint16_t lora_uart_read(uint8_t *buf, uint16_t max_len)
{
    uint16_t count = 0U;

    while (count < max_len && s_rx_tail != s_rx_head) {
        buf[count++] = s_rx_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) % LORA_RX_BUF_SIZE);
    }
    return count;
}

static void lora_uart_flush_rx(void)
{
    s_rx_tail = s_rx_head;
}

static void lora_reset_module(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50U);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(500U);
    lora_update_pins();
}

static HAL_StatusTypeDef lora_wait_aux(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) != GPIO_PIN_SET) {
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms) {
            return HAL_TIMEOUT;
        }
        HAL_Delay(1U);
    }
    return HAL_OK;
}

static void lora_copy_response(const char *src)
{
    size_t len = strlen(src);

    if (len >= sizeof(g_lora_test.last_response)) {
        len = sizeof(g_lora_test.last_response) - 1U;
    }
    memcpy(g_lora_test.last_response, src, len);
    g_lora_test.last_response[len] = '\0';
}

static HAL_StatusTypeDef lora_send_at(const char *cmd)
{
    char response[sizeof(g_lora_test.last_response)];
    uint16_t pos = 0U;
    uint32_t start;

    memset(response, 0, sizeof(response));
    strncpy(g_lora_test.last_command, cmd, sizeof(g_lora_test.last_command) - 1U);
    g_lora_test.last_command[sizeof(g_lora_test.last_command) - 1U] = '\0';
    lora_uart_flush_rx();

    if (HAL_UART_Transmit(&huart5, (uint8_t *)cmd, (uint16_t)strlen(cmd), 200U) != HAL_OK) {
        g_lora_test.last_status = HAL_ERROR;
        g_lora_test.at_error_count++;
        return HAL_ERROR;
    }

    start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < LORA_RESPONSE_TIMEOUT_MS) {
        uint8_t byte;
        uint16_t n = lora_uart_read(&byte, 1U);

        if (n > 0U) {
            if (pos < sizeof(response) - 1U) {
                response[pos++] = (char)byte;
                response[pos] = '\0';
            }
            if (strstr(response, "AT_OK") != NULL || strstr(response, "OK") != NULL) {
                lora_copy_response(response);
                g_lora_test.last_status = HAL_OK;
                g_lora_test.at_ok_count++;
                return HAL_OK;
            }
            if (strstr(response, "AT_PARAM_ERROR") != NULL || strstr(response, "AT_ERROR") != NULL) {
                lora_copy_response(response);
                g_lora_test.last_status = HAL_ERROR;
                g_lora_test.at_error_count++;
                return HAL_ERROR;
            }
        } else {
            HAL_Delay(1U);
        }
    }

    lora_copy_response(response);
    g_lora_test.last_status = HAL_TIMEOUT;
    g_lora_test.at_error_count++;
    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef lora_run_config(void)
{
    char cmd[32];
    HAL_StatusTypeDef st;

    g_lora_test.at_step = 0U;
    lora_reset_module();
    st = lora_wait_aux(LORA_RESPONSE_TIMEOUT_MS);
    if (st != HAL_OK) {
        g_lora_test.last_status = st;
        return st;
    }

    g_lora_test.at_step = 1U;
    st = lora_send_at("AT+HMODE=0");
    if (st != HAL_OK) { return st; }
    HAL_Delay(1000U);

    g_lora_test.at_step = 2U;
    st = lora_send_at("ATE0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 3U;
    st = lora_send_at("AT+COMSW=1,1,1");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 4U;
    st = lora_send_at("AT+RATE=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 5U;
    st = lora_send_at("AT+PACKET=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 6U;
    st = lora_send_at("AT+TRANS=1");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 7U;
    snprintf(cmd, sizeof(cmd), "AT+ADDR=%u", (unsigned)LORA_ADDR);
    st = lora_send_at(cmd);
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 8U;
    snprintf(cmd, sizeof(cmd), "AT+CHANNEL=%u", (unsigned)LORA_CHANNEL);
    st = lora_send_at(cmd);
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 9U;
    snprintf(cmd, sizeof(cmd), "AT+NETID=%u", (unsigned)LORA_NET_ID);
    st = lora_send_at(cmd);
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 10U;
    snprintf(cmd, sizeof(cmd), "AT+KEY=%u", (unsigned)LORA_KEY);
    st = lora_send_at(cmd);
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 11U;
    st = lora_send_at("AT+WOR=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 12U;
    st = lora_send_at("AT+ERSSI=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 13U;
    st = lora_send_at("AT+DRSSI=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 14U;
    st = lora_send_at("AT+LBT=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 15U;
    st = lora_send_at("AT+ROUTER=0");
    if (st != HAL_OK) { return st; }

    g_lora_test.at_step = 16U;
    st = lora_send_at("AT+HMODE=1");
    HAL_Delay(1000U);
    return st;
}

static HAL_StatusTypeDef lora_send_test_payload(void)
{
    static const uint8_t payload[] = "Team 15 PTSD\r\n";
    uint8_t frame[3U + sizeof(payload) - 1U];
    uint16_t dest = LORA_FIXED_DEST_ADDR;
    HAL_StatusTypeDef st;

    frame[0] = (uint8_t)(dest >> 8);
    frame[1] = (uint8_t)(dest & 0xFFU);
    frame[2] = LORA_CHANNEL;
    memcpy(&frame[3], payload, sizeof(payload) - 1U);
    memcpy(g_lora_test.last_payload, payload, sizeof(payload));

    st = lora_wait_aux(LORA_RESPONSE_TIMEOUT_MS);
    if (st != HAL_OK) {
        g_lora_test.last_status = st;
        return st;
    }

    st = HAL_UART_Transmit(&huart5, frame, (uint16_t)sizeof(frame), 200U);
    g_lora_test.last_status = st;
    if (st == HAL_OK) {
        g_lora_test.tx_count++;
    } else {
        g_lora_test.error_count++;
    }
    return st;
}

static void lora_drain_rx_to_watch(void)
{
    uint8_t byte;

    while (lora_uart_read(&byte, 1U) > 0U) {
        size_t len = strlen(g_lora_test.last_rx);

        if (byte == '\r' || byte == '\n') {
            continue;
        }
        if (len + 1U >= sizeof(g_lora_test.last_rx)) {
            memmove(g_lora_test.last_rx, &g_lora_test.last_rx[1], sizeof(g_lora_test.last_rx) - 2U);
            len = sizeof(g_lora_test.last_rx) - 2U;
            g_lora_test.last_rx[len] = '\0';
        }
        g_lora_test.last_rx[len] = (char)byte;
        g_lora_test.last_rx[len + 1U] = '\0';
    }
}

static void lora_process_command(void)
{
    uint32_t command = g_lora_test.command;

    if (command == LORA_TEST_CMD_NONE) {
        return;
    }
    g_lora_test.command = LORA_TEST_CMD_NONE;

    if (command == LORA_TEST_CMD_STATUS) {
        lora_update_pins();
    } else if (command == LORA_TEST_CMD_RESET) {
        lora_reset_module();
    } else if (command == LORA_TEST_CMD_CONFIG) {
        (void)lora_run_config();
    } else if (command == LORA_TEST_CMD_SEND) {
        (void)lora_send_test_payload();
    } else if (command == LORA_TEST_CMD_WAKE_LOW) {
        HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, GPIO_PIN_RESET);
        lora_update_pins();
    } else if (command == LORA_TEST_CMD_WAKE_HIGH) {
        HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, GPIO_PIN_SET);
        lora_update_pins();
    } else if (command == LORA_TEST_CMD_AUTO_OFF) {
        g_lora_test.auto_send = 0U;
    } else if (command == LORA_TEST_CMD_AUTO_ON) {
        g_lora_test.auto_send = 1U;
        s_last_auto_send_ms = 0U;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    memset(&g_lora_test, 0, sizeof(g_lora_test));
    g_lora_test.magic = LORA_TEST_MAGIC;
    g_lora_test.last_status = HAL_OK;

    MX_UART5_Init();
    lora_uart_rx_start();
    lora_update_pins();
    lora_reset_module();
    if (lora_run_config() == HAL_OK) {
        g_lora_test.auto_send = 1U;
        s_last_auto_send_ms = 0U;
    }

    while (1) {
        g_lora_test.uptime_ms = HAL_GetTick();
        lora_update_pins();
        lora_drain_rx_to_watch();
        lora_process_command();
        if (g_lora_test.auto_send != 0U &&
            (s_last_auto_send_ms == 0U ||
             (uint32_t)(HAL_GetTick() - s_last_auto_send_ms) >= LORA_AUTO_SEND_PERIOD_MS)) {
            if (lora_send_test_payload() == HAL_OK) {
                s_last_auto_send_ms = HAL_GetTick();
            }
        }
        HAL_Delay(10U);
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_UART5_Init(void)
{
    huart5.Instance = UART5;
    huart5.Init.BaudRate = 115200;
    huart5.Init.WordLength = UART_WORDLENGTH_8B;
    huart5.Init.StopBits = UART_STOPBITS_1;
    huart5.Init.Parity = UART_PARITY_NONE;
    huart5.Init.Mode = UART_MODE_TX_RX;
    huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart5) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LORA_LINK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LORA_LINK_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LORA_WAKE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_WAKE_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LORA_AUX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LORA_AUX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LORA_RST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_RST_GPIO_Port, &GPIO_InitStruct);
}

static void lora_uart_rx_start(void)
{
    if (HAL_UART_Receive_IT(&huart5, (uint8_t *)&s_rx_byte, 1U) != HAL_OK) {
        HAL_UART_AbortReceive_IT(&huart5);
        (void)HAL_UART_Receive_IT(&huart5, (uint8_t *)&s_rx_byte, 1U);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART5) {
        uint16_t next = (uint16_t)((s_rx_head + 1U) % LORA_RX_BUF_SIZE);
        g_lora_test.rx_count++;
        if (next != s_rx_tail) {
            s_rx_buf[s_rx_head] = s_rx_byte;
            s_rx_head = next;
        }
        lora_uart_rx_start();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART5) {
        g_lora_test.error_count++;
        __HAL_UART_CLEAR_OREFLAG(huart);
        lora_uart_rx_start();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
