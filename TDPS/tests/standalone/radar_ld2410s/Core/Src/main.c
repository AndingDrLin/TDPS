#include "main.h"
#include "ld2410s.h"
#include <string.h>

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

#define FILTER_SIZE               2U
#define DISTANCE_FRAME_MAX_CM     120U
#define LEFT_BLOCK_MIN_CM         15U
#define LEFT_BLOCK_MAX_CM         100U
#define RADAR_STALE_TIMEOUT_MS    300U
#define DEBUG_REPORT_PERIOD_MS    200U

typedef struct
{
    uint8_t uart_fresh;
    uint8_t frame_type;
    uint8_t uart_target;
    uint8_t target_state;
    uint8_t gpio_target;
    uint8_t left_obstacle_present;
    uint8_t path_turn_right;
    uint16_t raw_distance_cm;
    uint16_t filtered_distance_cm;
    uint32_t frame_age_ms;
    uint32_t frame_count;
    uint32_t raw_rx_bytes;
    uint32_t parse_error_count;
    uint32_t rx_overflow_count;
    HAL_StatusTypeDef last_rx_status;
} RadarWatchInfo;

LD2410S_FrameInfo g_frame_info;
volatile RadarWatchInfo g_radar_watch;
uint32_t g_last_frame_tick = 0U;
uint32_t g_last_report_tick = 0U;
uint8_t g_left_obstacle_present = 0U;
uint8_t g_path_turn_right = 0U;
uint8_t g_last_gpio_target = 0U;
uint16_t g_filtered_distance_cm = 0U;
uint16_t g_raw_distance_cm = 0U;

static uint16_t filter_buffer[FILTER_SIZE] = {0};
static uint8_t filter_index = 0U;
static uint8_t filter_count = 0U;
static uint32_t filter_sum = 0U;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);

static uint8_t RadarGpioTargetPresent(void)
{
    return (HAL_GPIO_ReadPin(RADAR_GPIO_GPIO_Port, RADAR_GPIO_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static uint8_t FrameHasTarget(const LD2410S_FrameInfo *info)
{
    return (info != NULL && info->target_state >= 2U) ? 1U : 0U;
}

static uint8_t LeftObstacleRaw(uint8_t has_target, uint16_t distance_cm)
{
    return (has_target &&
            distance_cm >= LEFT_BLOCK_MIN_CM &&
            distance_cm <= LEFT_BLOCK_MAX_CM) ? 1U : 0U;
}

static void ResetFilter(void)
{
    filter_index = 0U;
    filter_count = 0U;
    filter_sum = 0U;
    memset(filter_buffer, 0, sizeof(filter_buffer));
}

static uint16_t FilterDistanceStable(uint16_t raw_distance)
{
    if ((raw_distance == 0U) || (raw_distance > DISTANCE_FRAME_MAX_CM))
    {
        if (filter_count == 0U)
        {
            return 0U;
        }
        return (uint16_t)(filter_sum / filter_count);
    }

    filter_sum -= filter_buffer[filter_index];
    filter_buffer[filter_index] = raw_distance;
    filter_sum += raw_distance;

    filter_index++;
    if (filter_index >= FILTER_SIZE)
    {
        filter_index = 0U;
    }

    if (filter_count < FILTER_SIZE)
    {
        filter_count++;
    }

    return (uint16_t)(filter_sum / filter_count);
}

static void ResetRadarTracking(void)
{
    g_left_obstacle_present = 0U;
    g_path_turn_right = 0U;
    g_filtered_distance_cm = 0U;
    ResetFilter();
}

static void UpdateRadarWatch(uint32_t now, uint8_t uart_fresh)
{
    g_radar_watch.uart_fresh = uart_fresh;
    g_radar_watch.frame_type = (uint8_t)g_frame_info.frame_type;
    g_radar_watch.uart_target = FrameHasTarget(&g_frame_info);
    g_radar_watch.target_state = g_frame_info.target_state;
    g_radar_watch.gpio_target = g_last_gpio_target;
    g_radar_watch.left_obstacle_present = g_left_obstacle_present;
    g_radar_watch.path_turn_right = g_path_turn_right;
    g_radar_watch.raw_distance_cm = g_raw_distance_cm;
    g_radar_watch.filtered_distance_cm = g_filtered_distance_cm;
    g_radar_watch.frame_age_ms = now - g_last_frame_tick;
    g_radar_watch.frame_count = LD2410S_FrameCount();
    g_radar_watch.raw_rx_bytes = LD2410S_RawRxBytes();
    g_radar_watch.parse_error_count = LD2410S_ParseErrorCount();
    g_radar_watch.rx_overflow_count = LD2410S_RxOverflowCount();
    g_radar_watch.last_rx_status = LD2410S_LastRxStatus();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();

    LD2410S_Init(&huart3, &huart2);
    LD2410S_StartReceiveIT();

    g_last_frame_tick = HAL_GetTick();
    g_last_report_tick = HAL_GetTick();
    Debug_Printf("RADAR TEST START\r\n");
    Debug_Printf("RADAR UART: USART3 PB10=TX PB11=RX baud=%lu\r\n", (unsigned long)huart3.Init.BaudRate);
    Debug_Printf("DEBUG UART: USART2 PA2=TX PA3=RX baud=%lu\r\n", (unsigned long)huart2.Init.BaudRate);
    Debug_Printf("RADAR GPIO: PB0 input, high=target_present\r\n");
    Debug_Printf("WATCH ONLY: add g_radar_watch in Keil Watch window\r\n");

    while (1)
    {
        uint32_t now = HAL_GetTick();
        uint8_t uart_fresh;
        uint8_t has_target;

        while (LD2410S_GetFrame(&g_frame_info))
        {
            g_last_frame_tick = now;
            g_raw_distance_cm = g_frame_info.distance_cm;
            has_target = FrameHasTarget(&g_frame_info);

            if (has_target &&
                (g_frame_info.distance_cm > 0U) &&
                (g_frame_info.distance_cm <= DISTANCE_FRAME_MAX_CM))
            {
                g_filtered_distance_cm = FilterDistanceStable(g_frame_info.distance_cm);
                g_left_obstacle_present = LeftObstacleRaw(1U, g_filtered_distance_cm);
            }
            else
            {
                g_filtered_distance_cm = 0U;
                g_left_obstacle_present = 0U;
                ResetFilter();
            }

            g_path_turn_right = g_left_obstacle_present;
        }

        uart_fresh = ((now - g_last_frame_tick) <= RADAR_STALE_TIMEOUT_MS) ? 1U : 0U;
        g_last_gpio_target = RadarGpioTargetPresent();
        if (!uart_fresh)
        {
            ResetRadarTracking();
        }
        UpdateRadarWatch(now, uart_fresh);

        if ((now - g_last_report_tick) >= DEBUG_REPORT_PERIOD_MS)
        {
            Debug_Printf("RADAR FRAME: valid=%u type=%u target=%u state=%u raw_cm=%u filt_cm=%u frames=%lu rx=%lu err=%lu ovf=%lu\r\n",
                         uart_fresh,
                         g_frame_info.frame_type,
                         FrameHasTarget(&g_frame_info),
                         g_frame_info.target_state,
                         g_raw_distance_cm,
                         g_filtered_distance_cm,
                         (unsigned long)LD2410S_FrameCount(),
                         (unsigned long)LD2410S_RawRxBytes(),
                         (unsigned long)LD2410S_ParseErrorCount(),
                         (unsigned long)LD2410S_RxOverflowCount());
            Debug_Printf("RADAR GPIO: PB0_TARGET=%u\r\n", g_last_gpio_target);
            if (!uart_fresh)
            {
                Debug_Printf("LEFT OBSTACLE: absent reason=RADAR_STALE\r\n");
                Debug_Printf("PATH CMD: WAIT reason=RADAR_STALE\r\n");
            }
            else
            {
                Debug_Printf("LEFT OBSTACLE: %s distance_cm=%u min_cm=%u max_cm=%u\r\n",
                             g_left_obstacle_present ? "present" : "absent",
                             g_filtered_distance_cm,
                             LEFT_BLOCK_MIN_CM,
                             LEFT_BLOCK_MAX_CM);
                Debug_Printf("PATH CMD: %s reason=%s\r\n",
                             g_path_turn_right ? "TURN_RIGHT" : "TURN_LEFT",
                             g_path_turn_right ? "LEFT_BLOCKED" : "LEFT_CLEAR");
            }
            g_last_report_tick = now;
        }

        HAL_Delay(1U);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = RADAR_GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(RADAR_GPIO_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
