/*
 * Standalone HLK-LD2410S radar test template for STM32F407VET6 CubeMX projects.
 *
 * Usage:
 * 1. Copy or merge this file into your CubeMX Core/Src/main.c.
 * 2. Keep your original CubeMX clock/GPIO/UART init functions and startup order.
 * 3. Update the USER CONFIG section below for your actual radar/debug UART handles.
 * 4. If your project already defines HAL_UART_RxCpltCallback or HAL_UART_ErrorCallback,
 *    merge only the radar callback blocks from this file into the existing callbacks.
 *
 * Wiring from HLK-LD2410S user manual V1.04 and current board assignment:
 * - J2 3V3 -> 3.3 V, J2 GND -> MCU GND
 * - J2 OT1 / UART_TX -> PB11 / MCU USART3_RX
 * - J2 RX / UART_RX  -> PB10 / MCU USART3_TX
 * - J2 OT2 / GPIO output -> PB0 input: high = target present, low = no target
 * - PC0 LED: UART fresh frame / communication OK
 * - PC1 LED: PB0 radar GPIO target status
 * - PC2 LED: left obstacle present by UART distance threshold
 * - PC3 LED: simulated path command TURN_RIGHT
 *
 * Serial output is ASCII-only for PC serial tools.
 */

#include "main.h"
#if defined(__has_include)
#if __has_include("gpio.h")
#include "gpio.h"
#endif
#if __has_include("usart.h")
#include "usart.h"
#endif
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ============================ USER CONFIG ============================ */

#define RADAR_UART_HANDLE huart3
#define DEBUG_UART_HANDLE huart1

#define RADAR_EXPECTED_BAUDRATE 115200U
#define RADAR_RX_BUFFER_SIZE 512U
#define DEBUG_TX_TIMEOUT_MS 50U

#define RADAR_TRIGGER_DISTANCE_MM 450U
#define RADAR_RELEASE_DISTANCE_MM 650U
#define LEFT_BLOCK_MIN_DISTANCE_MM 150U
#define LEFT_BLOCK_DISTANCE_MM 1000U
#define RADAR_FRAME_TIMEOUT_MS 300U
#define RADAR_DEBOUNCE_FRAMES 3U
#define RADAR_LOG_PERIOD_MS 200U

#define RADAR_GPIO_PORT GPIOB
#define RADAR_GPIO_PIN GPIO_PIN_0
#define LED_UART_FRESH_PORT GPIOC
#define LED_UART_FRESH_PIN GPIO_PIN_0
#define LED_GPIO_TARGET_PORT GPIOC
#define LED_GPIO_TARGET_PIN GPIO_PIN_1
#define LED_LEFT_OBSTACLE_PORT GPIOC
#define LED_LEFT_OBSTACLE_PIN GPIO_PIN_2
#define LED_TURN_RIGHT_PORT GPIOC
#define LED_TURN_RIGHT_PIN GPIO_PIN_3
#define LED_ACTIVE_STATE GPIO_PIN_SET
#define LED_INACTIVE_STATE GPIO_PIN_RESET
#define RADAR_TEST_FORCE_GPIO_CONFIG 1

#define RADAR_TEST_INIT_GPIO() MX_GPIO_Init()
#define RADAR_TEST_INIT_DEBUG_UART() MX_USART1_UART_Init()
#define RADAR_TEST_INIT_RADAR_UART() MX_USART3_UART_Init()
#define RADAR_TEST_DEFINE_ERROR_HANDLER 0

/* If your CubeMX project uses different generated names, update these declarations too. */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART3_UART_Init(void);

/* =========================== RADAR PARSER ============================ */

#define RADAR_F4_HEADER_SIZE 4U
#define RADAR_SIMPLE_PAYLOAD_SIZE 4U
#define RADAR_LD2410S_MINIMAL_SIZE 5U
#define RADAR_LD2410S_STANDARD_SIZE 70U
#define RADAR_CM_TO_MM 10U

static const uint8_t k_f4_frame_header[RADAR_F4_HEADER_SIZE] = {0xF4U, 0xF3U, 0xF2U, 0xF1U};

typedef enum {
    RADAR_OBSTACLE_CLEAR = 0,
    RADAR_OBSTACLE_WARN,
    RADAR_OBSTACLE_BLOCK,
} RadarObstacleState;

typedef enum {
    RADAR_FRAME_NONE = 0,
    RADAR_FRAME_SIMPLE,
    RADAR_FRAME_LD2410S_MINIMAL,
    RADAR_FRAME_LD2410S_STANDARD,
} RadarFrameType;

typedef enum {
    RADAR_PARSE_SEARCH = 0,
    RADAR_PARSE_F4_HEADER,
    RADAR_PARSE_F4_BODY,
    RADAR_PARSE_LD2410S_MINIMAL,
} RadarParseMode;

typedef struct {
    bool has_target;
    bool frame_valid;
    bool left_obstacle_present;
    uint8_t target_state;
    uint16_t distance_mm;
    uint32_t frame_count;
    uint32_t parse_error_count;
    uint32_t last_update_ms;
    RadarFrameType frame_type;
    RadarObstacleState near_obstacle_state;

    RadarParseMode parse_mode;
    uint8_t sync_index;
    uint8_t payload_index;
    uint8_t payload[RADAR_SIMPLE_PAYLOAD_SIZE];
    uint8_t f4_frame[RADAR_LD2410S_STANDARD_SIZE];
    uint8_t f4_index;
    uint8_t minimal_frame[RADAR_LD2410S_MINIMAL_SIZE];
    uint8_t minimal_index;

    uint8_t danger_count;
    uint8_t warn_count;
    uint8_t clear_count;
    uint8_t left_present_count;
    uint8_t left_absent_count;
} RadarRuntime;

static RadarRuntime s_radar;

static volatile uint8_t s_radar_rx_buf[RADAR_RX_BUFFER_SIZE];
static volatile uint16_t s_radar_rx_head;
static volatile uint16_t s_radar_rx_tail;
static volatile uint8_t s_radar_rx_byte;
static volatile uint32_t s_raw_rx_bytes;
static volatile uint32_t s_rx_overflow_count;
static HAL_StatusTypeDef s_last_rx_start_status = HAL_OK;
static uint32_t s_last_log_ms;

static uint8_t debounce_frames(void)
{
    uint32_t frames = RADAR_DEBOUNCE_FRAMES;

    if (frames < 1U) {
        return 1U;
    }
    if (frames > 20U) {
        return 20U;
    }
    return (uint8_t)frames;
}

static uint16_t cm_to_mm(uint16_t distance_cm)
{
    uint32_t distance_mm = (uint32_t)distance_cm * RADAR_CM_TO_MM;

    if (distance_mm > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t)distance_mm;
}

static void reset_parser(void)
{
    s_radar.parse_mode = RADAR_PARSE_SEARCH;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
    s_radar.minimal_index = 0U;
}

static void reset_near_obstacle_state(void)
{
    s_radar.danger_count = 0U;
    s_radar.warn_count = 0U;
    s_radar.clear_count = 0U;
    s_radar.near_obstacle_state = RADAR_OBSTACLE_CLEAR;
}

static bool simple_payload_checksum_ok(void)
{
    uint8_t checksum = (uint8_t)(s_radar.payload[0] ^ s_radar.payload[1] ^ s_radar.payload[2]);
    return checksum == s_radar.payload[3];
}

static bool f4_payload_is_standard_candidate(void)
{
    return s_radar.payload[0] == 0x46U &&
           s_radar.payload[1] == 0x00U &&
           s_radar.payload[2] == 0x01U;
}

static void update_near_obstacle_state(bool has_target, uint16_t distance_mm)
{
    const uint8_t debounce = debounce_frames();

    if (has_target && distance_mm <= RADAR_TRIGGER_DISTANCE_MM) {
        if (s_radar.danger_count < 255U) {
            s_radar.danger_count += 1U;
        }
        s_radar.clear_count = 0U;

        if (s_radar.danger_count >= debounce) {
            s_radar.near_obstacle_state = RADAR_OBSTACLE_BLOCK;
        }
        return;
    }

    s_radar.danger_count = 0U;

    if (s_radar.near_obstacle_state == RADAR_OBSTACLE_BLOCK) {
        if ((!has_target) || distance_mm >= RADAR_RELEASE_DISTANCE_MM) {
            if (s_radar.clear_count < 255U) {
                s_radar.clear_count += 1U;
            }
            if (s_radar.clear_count >= debounce) {
                s_radar.near_obstacle_state = RADAR_OBSTACLE_CLEAR;
                s_radar.clear_count = 0U;
            }
        } else {
            s_radar.clear_count = 0U;
        }
        return;
    }

    if (has_target && distance_mm <= RADAR_RELEASE_DISTANCE_MM) {
        if (s_radar.warn_count < 255U) {
            s_radar.warn_count += 1U;
        }
        s_radar.clear_count = 0U;
        if (s_radar.warn_count >= debounce) {
            s_radar.near_obstacle_state = RADAR_OBSTACLE_WARN;
        }
    } else {
        s_radar.warn_count = 0U;
        if (s_radar.near_obstacle_state == RADAR_OBSTACLE_WARN) {
            if (s_radar.clear_count < 255U) {
                s_radar.clear_count += 1U;
            }
            if (s_radar.clear_count >= debounce) {
                s_radar.near_obstacle_state = RADAR_OBSTACLE_CLEAR;
                s_radar.clear_count = 0U;
            }
        } else {
            s_radar.near_obstacle_state = RADAR_OBSTACLE_CLEAR;
            s_radar.clear_count = 0U;
        }
    }
}

static bool left_obstacle_raw(bool has_target, uint16_t distance_mm)
{
    return has_target &&
           distance_mm >= LEFT_BLOCK_MIN_DISTANCE_MM &&
           distance_mm <= LEFT_BLOCK_DISTANCE_MM;
}

static void update_left_obstacle_state(bool has_target, uint16_t distance_mm)
{
    const uint8_t debounce = debounce_frames();

    if (left_obstacle_raw(has_target, distance_mm)) {
        if (s_radar.left_present_count < 255U) {
            s_radar.left_present_count += 1U;
        }
        s_radar.left_absent_count = 0U;
        if (s_radar.left_present_count >= debounce) {
            s_radar.left_obstacle_present = true;
        }
        return;
    }

    if (s_radar.left_absent_count < 255U) {
        s_radar.left_absent_count += 1U;
    }
    s_radar.left_present_count = 0U;
    if (s_radar.left_absent_count >= debounce) {
        s_radar.left_obstacle_present = false;
    }
}

static void consume_measurement(uint32_t now_ms,
                                bool has_target,
                                uint16_t distance_mm,
                                uint8_t target_state,
                                RadarFrameType frame_type)
{
    s_radar.frame_valid = true;
    s_radar.has_target = has_target;
    s_radar.target_state = target_state;
    s_radar.distance_mm = distance_mm;
    s_radar.frame_type = frame_type;
    s_radar.frame_count += 1U;
    s_radar.last_update_ms = now_ms;

    update_near_obstacle_state(has_target, distance_mm);
    update_left_obstacle_state(has_target, distance_mm);
}

static void consume_simple_frame(uint32_t now_ms)
{
    uint16_t distance_mm;
    uint8_t target_state;

    if (!simple_payload_checksum_ok()) {
        s_radar.parse_error_count += 1U;
        return;
    }

    distance_mm = (uint16_t)(((uint16_t)s_radar.payload[1] << 8) | (uint16_t)s_radar.payload[0]);
    target_state = s_radar.payload[2];
    consume_measurement(now_ms,
                        target_state != 0U,
                        distance_mm,
                        target_state,
                        RADAR_FRAME_SIMPLE);
}

static void consume_ld2410s_minimal_frame(uint32_t now_ms)
{
    uint8_t target_state;
    uint16_t distance_cm;

    if (s_radar.minimal_frame[4] != 0x62U) {
        s_radar.parse_error_count += 1U;
        return;
    }

    target_state = s_radar.minimal_frame[1];
    distance_cm = (uint16_t)(s_radar.minimal_frame[2] |
                             ((uint16_t)s_radar.minimal_frame[3] << 8));
    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        RADAR_FRAME_LD2410S_MINIMAL);
}

static void consume_ld2410s_standard_frame(uint32_t now_ms)
{
    uint8_t target_state;
    uint16_t distance_cm;

    if (s_radar.f4_index != RADAR_LD2410S_STANDARD_SIZE ||
        s_radar.f4_frame[0] != 0xF4U ||
        s_radar.f4_frame[1] != 0xF3U ||
        s_radar.f4_frame[2] != 0xF2U ||
        s_radar.f4_frame[3] != 0xF1U ||
        s_radar.f4_frame[4] != 0x46U ||
        s_radar.f4_frame[5] != 0x00U ||
        s_radar.f4_frame[6] != 0x01U ||
        s_radar.f4_frame[66] != 0xF8U ||
        s_radar.f4_frame[67] != 0xF7U ||
        s_radar.f4_frame[68] != 0xF6U ||
        s_radar.f4_frame[69] != 0xF5U) {
        s_radar.parse_error_count += 1U;
        return;
    }

    target_state = s_radar.f4_frame[7];
    distance_cm = (uint16_t)(s_radar.f4_frame[8] |
                             ((uint16_t)s_radar.f4_frame[9] << 8));
    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        RADAR_FRAME_LD2410S_STANDARD);
}

static void start_minimal_frame(void)
{
    s_radar.parse_mode = RADAR_PARSE_LD2410S_MINIMAL;
    s_radar.minimal_frame[0] = 0x6EU;
    s_radar.minimal_index = 1U;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
}

static void start_f4_header(void)
{
    s_radar.parse_mode = RADAR_PARSE_F4_HEADER;
    s_radar.f4_frame[0] = 0xF4U;
    s_radar.f4_index = 1U;
    s_radar.sync_index = 1U;
    s_radar.payload_index = 0U;
    s_radar.minimal_index = 0U;
}

static void parse_search_byte(uint8_t b)
{
    if (b == 0x6EU) {
        start_minimal_frame();
    } else if (b == k_f4_frame_header[0]) {
        start_f4_header();
    }
}

static void parse_f4_header_byte(uint8_t b)
{
    if (b == k_f4_frame_header[s_radar.sync_index]) {
        s_radar.f4_frame[s_radar.f4_index++] = b;
        s_radar.sync_index += 1U;
        if (s_radar.sync_index >= RADAR_F4_HEADER_SIZE) {
            s_radar.parse_mode = RADAR_PARSE_F4_BODY;
            s_radar.payload_index = 0U;
        }
        return;
    }

    if (b == 0x6EU) {
        start_minimal_frame();
    } else if (b == k_f4_frame_header[0]) {
        start_f4_header();
    } else {
        reset_parser();
    }
}

static void parse_f4_body_byte(uint8_t b, uint32_t now_ms)
{
    if (s_radar.f4_index >= RADAR_LD2410S_STANDARD_SIZE) {
        s_radar.parse_error_count += 1U;
        reset_parser();
        parse_search_byte(b);
        return;
    }

    s_radar.f4_frame[s_radar.f4_index++] = b;

    if (s_radar.payload_index < RADAR_SIMPLE_PAYLOAD_SIZE) {
        s_radar.payload[s_radar.payload_index++] = b;
    }

    if (s_radar.payload_index < RADAR_SIMPLE_PAYLOAD_SIZE) {
        return;
    }

    if (f4_payload_is_standard_candidate()) {
        if (s_radar.f4_index >= RADAR_LD2410S_STANDARD_SIZE) {
            consume_ld2410s_standard_frame(now_ms);
            reset_parser();
        }
        return;
    }

    if (simple_payload_checksum_ok()) {
        consume_simple_frame(now_ms);
        reset_parser();
        return;
    }

    s_radar.parse_error_count += 1U;
    reset_parser();
    parse_search_byte(b);
}

static void parse_minimal_byte(uint8_t b, uint32_t now_ms)
{
    s_radar.minimal_frame[s_radar.minimal_index++] = b;
    if (s_radar.minimal_index >= RADAR_LD2410S_MINIMAL_SIZE) {
        consume_ld2410s_minimal_frame(now_ms);
        reset_parser();
    }
}

static void radar_parser_push_byte(uint8_t b, uint32_t now_ms)
{
    switch (s_radar.parse_mode) {
    case RADAR_PARSE_SEARCH:
        parse_search_byte(b);
        break;

    case RADAR_PARSE_F4_HEADER:
        parse_f4_header_byte(b);
        break;

    case RADAR_PARSE_F4_BODY:
        parse_f4_body_byte(b, now_ms);
        break;

    case RADAR_PARSE_LD2410S_MINIMAL:
        parse_minimal_byte(b, now_ms);
        break;

    default:
        reset_parser();
        break;
    }
}

static void radar_parser_init(void)
{
    memset(&s_radar, 0, sizeof(s_radar));
    s_radar.near_obstacle_state = RADAR_OBSTACLE_CLEAR;
    s_radar.frame_type = RADAR_FRAME_NONE;
}

static bool radar_frame_fresh(uint32_t now_ms)
{
    return s_radar.frame_valid &&
           ((uint32_t)(now_ms - s_radar.last_update_ms) <= RADAR_FRAME_TIMEOUT_MS);
}

static void radar_parser_tick_timeout(uint32_t now_ms)
{
    if (s_radar.frame_valid &&
        ((uint32_t)(now_ms - s_radar.last_update_ms) > RADAR_FRAME_TIMEOUT_MS)) {
        reset_near_obstacle_state();
        s_radar.has_target = false;
        s_radar.frame_valid = false;
        s_radar.target_state = 0U;
        s_radar.distance_mm = 0U;
        s_radar.frame_type = RADAR_FRAME_NONE;
    }
}

/* ========================= GPIO / UART / LOG ========================== */

static void set_led(GPIO_TypeDef *port, uint16_t pin, bool on)
{
    HAL_GPIO_WritePin(port, pin, on ? LED_ACTIVE_STATE : LED_INACTIVE_STATE);
}

static bool radar_gpio_target_present(void)
{
    return HAL_GPIO_ReadPin(RADAR_GPIO_PORT, RADAR_GPIO_PIN) == GPIO_PIN_SET;
}

static void radar_test_config_gpio_pins(void)
{
#if RADAR_TEST_FORCE_GPIO_CONFIG
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio_init.Pin = RADAR_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(RADAR_GPIO_PORT, &gpio_init);

    gpio_init.Pin = LED_UART_FRESH_PIN |
                    LED_GPIO_TARGET_PIN |
                    LED_LEFT_OBSTACLE_PIN |
                    LED_TURN_RIGHT_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio_init);
#endif

    set_led(LED_UART_FRESH_PORT, LED_UART_FRESH_PIN, false);
    set_led(LED_GPIO_TARGET_PORT, LED_GPIO_TARGET_PIN, false);
    set_led(LED_LEFT_OBSTACLE_PORT, LED_LEFT_OBSTACLE_PIN, false);
    set_led(LED_TURN_RIGHT_PORT, LED_TURN_RIGHT_PIN, false);
}

static void radar_test_update_leds(uint32_t now_ms)
{
    bool fresh = radar_frame_fresh(now_ms);
    bool gpio_target = radar_gpio_target_present();
    bool turn_right = fresh && s_radar.left_obstacle_present;

    set_led(LED_UART_FRESH_PORT, LED_UART_FRESH_PIN, fresh);
    set_led(LED_GPIO_TARGET_PORT, LED_GPIO_TARGET_PIN, gpio_target);
    set_led(LED_LEFT_OBSTACLE_PORT, LED_LEFT_OBSTACLE_PIN, fresh && s_radar.left_obstacle_present);
    set_led(LED_TURN_RIGHT_PORT, LED_TURN_RIGHT_PIN, turn_right);
}

static const char *hal_status_name(HAL_StatusTypeDef status)
{
    switch (status) {
    case HAL_OK:
        return "OK";
    case HAL_ERROR:
        return "ERROR";
    case HAL_BUSY:
        return "BUSY";
    case HAL_TIMEOUT:
        return "TIMEOUT";
    default:
        return "UNKNOWN";
    }
}

static const char *frame_type_name(RadarFrameType type)
{
    switch (type) {
    case RADAR_FRAME_SIMPLE:
        return "SIMPLE";
    case RADAR_FRAME_LD2410S_MINIMAL:
        return "LD2410S_MIN";
    case RADAR_FRAME_LD2410S_STANDARD:
        return "LD2410S_STD";
    case RADAR_FRAME_NONE:
    default:
        return "NONE";
    }
}

static const char *near_obstacle_name(RadarObstacleState state)
{
    switch (state) {
    case RADAR_OBSTACLE_WARN:
        return "WARN";
    case RADAR_OBSTACLE_BLOCK:
        return "BLOCK";
    case RADAR_OBSTACLE_CLEAR:
    default:
        return "CLEAR";
    }
}

static void debug_printf(const char *fmt, ...)
{
    char msg[256];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (len <= 0) {
        return;
    }
    if ((size_t)len >= sizeof(msg)) {
        len = (int)sizeof(msg) - 1;
    }

    (void)HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)msg, (uint16_t)len, DEBUG_TX_TIMEOUT_MS);
}

static HAL_StatusTypeDef radar_rx_start(void)
{
    HAL_StatusTypeDef status;

    status = HAL_UART_Receive_IT(&RADAR_UART_HANDLE, (uint8_t *)&s_radar_rx_byte, 1U);
    if (status != HAL_OK) {
        (void)HAL_UART_AbortReceive_IT(&RADAR_UART_HANDLE);
        status = HAL_UART_Receive_IT(&RADAR_UART_HANDLE, (uint8_t *)&s_radar_rx_byte, 1U);
    }
    s_last_rx_start_status = status;
    return status;
}

static bool radar_rx_read_byte(uint8_t *out)
{
    if (out == NULL || s_radar_rx_tail == s_radar_rx_head) {
        return false;
    }

    *out = s_radar_rx_buf[s_radar_rx_tail];
    s_radar_rx_tail = (uint16_t)((s_radar_rx_tail + 1U) % RADAR_RX_BUFFER_SIZE);
    return true;
}

static void radar_test_print_init(HAL_StatusTypeDef rx_status)
{
    uint32_t radar_baud = RADAR_UART_HANDLE.Init.BaudRate;
    uint32_t debug_baud = DEBUG_UART_HANDLE.Init.BaudRate;
    const char *init_state = "OK";

    if (rx_status != HAL_OK) {
        init_state = "ERROR";
    } else if (radar_baud != RADAR_EXPECTED_BAUDRATE) {
        init_state = "WARN";
    }

    debug_printf("RADAR TEST START\r\n");
    debug_printf("RADAR MANUAL: HLK-LD2410S UART default baud=115200 voltage=3.3V\r\n");
    debug_printf("RADAR WIRING: OT1_TX_TO_PB11_USART3_RX RX_TO_PB10_USART3_TX OT2_TO_PB0 COMMON_GND VCC_3V3\r\n");
    debug_printf("LED MAP: PC0=UART_FRESH PC1=PB0_TARGET PC2=LEFT_OBSTACLE PC3=TURN_RIGHT\r\n");
    debug_printf("RADAR INIT: %s radar_baud=%lu expected_baud=%lu debug_baud=%lu rx_it=%s\r\n",
                 init_state,
                 (unsigned long)radar_baud,
                 (unsigned long)RADAR_EXPECTED_BAUDRATE,
                 (unsigned long)debug_baud,
                 hal_status_name(rx_status));

    if (radar_baud != RADAR_EXPECTED_BAUDRATE) {
        debug_printf("RADAR WARN: Set CubeMX radar UART baud to 115200 unless the sensor was reconfigured.\r\n");
    }
}

static void radar_test_init(void)
{
    HAL_StatusTypeDef rx_status;

    s_radar_rx_head = 0U;
    s_radar_rx_tail = 0U;
    s_raw_rx_bytes = 0U;
    s_rx_overflow_count = 0U;
    radar_parser_init();
    radar_test_config_gpio_pins();
    rx_status = radar_rx_start();
    s_last_log_ms = HAL_GetTick();
    radar_test_print_init(rx_status);
}

static void radar_test_log(uint32_t now_ms)
{
    bool fresh = radar_frame_fresh(now_ms);
    bool gpio_target = radar_gpio_target_present();
    bool raw_left_present = fresh && left_obstacle_raw(s_radar.has_target, s_radar.distance_mm);
    uint32_t age_ms = fresh ? (uint32_t)(now_ms - s_radar.last_update_ms) : 999999U;

    debug_printf("RADAR FRAME: valid=%u fresh=%u type=%s target=%u target_state=%u distance_mm=%u age_ms=%lu frames=%lu errors=%lu raw_rx_bytes=%lu rx_overflow=%lu rx_it=%s\r\n",
                 s_radar.frame_valid ? 1U : 0U,
                 fresh ? 1U : 0U,
                 frame_type_name(s_radar.frame_type),
                 s_radar.has_target ? 1U : 0U,
                 (unsigned int)s_radar.target_state,
                 (unsigned int)s_radar.distance_mm,
                 (unsigned long)age_ms,
                 (unsigned long)s_radar.frame_count,
                 (unsigned long)s_radar.parse_error_count,
                 (unsigned long)s_raw_rx_bytes,
                 (unsigned long)s_rx_overflow_count,
                 hal_status_name(s_last_rx_start_status));

    debug_printf("NEAR OBSTACLE: %s distance_mm=%u trigger_mm=%u release_mm=%u\r\n",
                 near_obstacle_name(s_radar.near_obstacle_state),
                 (unsigned int)s_radar.distance_mm,
                 (unsigned int)RADAR_TRIGGER_DISTANCE_MM,
                 (unsigned int)RADAR_RELEASE_DISTANCE_MM);
    debug_printf("RADAR GPIO: PB0_TARGET=%u\r\n", gpio_target ? 1U : 0U);

    if (!fresh) {
        debug_printf("LEFT OBSTACLE: absent reason=NO_FRESH_FRAME\r\n");
        debug_printf("PATH CMD: WAIT reason=RADAR_STALE\r\n");
        return;
    }

    debug_printf("LEFT OBSTACLE: %s raw=%s distance_mm=%u min_mm=%u threshold_mm=%u confirm_present=%u confirm_absent=%u\r\n",
                 s_radar.left_obstacle_present ? "present" : "absent",
                 raw_left_present ? "present" : "absent",
                 (unsigned int)s_radar.distance_mm,
                 (unsigned int)LEFT_BLOCK_MIN_DISTANCE_MM,
                 (unsigned int)LEFT_BLOCK_DISTANCE_MM,
                 (unsigned int)s_radar.left_present_count,
                 (unsigned int)s_radar.left_absent_count);

    if (s_radar.left_obstacle_present) {
        debug_printf("PATH CMD: TURN_RIGHT reason=LEFT_BLOCKED\r\n");
    } else {
        debug_printf("PATH CMD: TURN_LEFT reason=LEFT_CLEAR\r\n");
    }
}

static void radar_test_task(void)
{
    uint8_t b;
    uint32_t now_ms = HAL_GetTick();

    while (radar_rx_read_byte(&b)) {
        radar_parser_push_byte(b, now_ms);
    }

    radar_parser_tick_timeout(now_ms);
    radar_test_update_leds(now_ms);

    if ((uint32_t)(now_ms - s_last_log_ms) >= RADAR_LOG_PERIOD_MS) {
        s_last_log_ms = now_ms;
        radar_test_log(now_ms);
    }
}

/* ============================== MAIN ================================= */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    RADAR_TEST_INIT_GPIO();
    RADAR_TEST_INIT_DEBUG_UART();
    RADAR_TEST_INIT_RADAR_UART();

    radar_test_init();

    while (1) {
        radar_test_task();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == RADAR_UART_HANDLE.Instance) {
        uint16_t next = (uint16_t)((s_radar_rx_head + 1U) % RADAR_RX_BUFFER_SIZE);

        s_raw_rx_bytes += 1U;
        if (next != s_radar_rx_tail) {
            s_radar_rx_buf[s_radar_rx_head] = s_radar_rx_byte;
            s_radar_rx_head = next;
        } else {
            s_rx_overflow_count += 1U;
        }

        (void)radar_rx_start();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != NULL && huart->Instance == RADAR_UART_HANDLE.Instance) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        (void)radar_rx_start();
    }
}

#if RADAR_TEST_DEFINE_ERROR_HANDLER
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
#endif
