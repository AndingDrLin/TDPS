/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ld2410s.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
LD2410S_FrameInfo g_frame_info;
uint32_t g_last_frame_tick = 0;
uint32_t g_last_report_tick = 0;
uint32_t g_heartbeat_tick = 0;
uint8_t g_left_obstacle_present = 0;
uint8_t g_path_turn_right = 0;
uint8_t g_last_gpio_target = 0;
uint16_t g_filtered_distance_cm = 0;
uint16_t g_raw_distance_cm = 0;
uint32_t g_last_oled_update_tick = 0;

#define FILTER_SIZE               2U
#define DISTANCE_MAX_CM           100U
#define DISTANCE_FRAME_MAX_CM     120U
#define LEFT_BLOCK_MIN_CM         15U
#define LEFT_BLOCK_MAX_CM         100U
#define DETECT_CONFIRM_FRAMES     1U
#define CLEAR_CONFIRM_FRAMES      1U
#define ZONE_CONFIRM_FRAMES       1U
#define DISTANCE_HYSTERESIS_CM    2U
#define RADAR_STALE_TIMEOUT_MS    300U
#define DEBUG_REPORT_PERIOD_MS    200U
#define OLED_UPDATE_PERIOD_MS     100U
#define OLED_USART_TIMEOUT_MS     50U
#define OLED_CURVE_DESC_PTR       0x6000U
#define OLED_CURVE_CHANNEL_MODE   0x01U
#define OLED_CURVE_CLEAR_SAMPLES  64U
#define OLED_CURVE_SAMPLE_MAX     1000U
static uint16_t filter_buffer[FILTER_SIZE] = {0};
static uint8_t filter_index = 0;
static uint8_t filter_count = 0;
static uint32_t filter_sum = 0;
static uint8_t detect_confirm_count = 0;
static uint8_t clear_confirm_count = 0;
static uint8_t active_zone = 0;
static uint8_t pending_zone = 0;
static uint8_t pending_zone_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_UART4_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// LED 鎺у埗鍑芥暟
static void LED_AllOff(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
}

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

static void LED_SetTestStatus(uint8_t uart_fresh, uint8_t gpio_target, uint8_t left_obstacle, uint8_t turn_right)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, uart_fresh ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, gpio_target ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, left_obstacle ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, turn_right ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static HAL_StatusTypeDef OLED_SendRaw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(&huart2, (uint8_t *)data, len, OLED_USART_TIMEOUT_MS);
}

static void OLED_TriggerCurveDisplay(void)
{
    /* 0x310 curve buffer data write trigger: 5A A5 06 82 3100 5AA5 0100 */
    static const uint8_t trigger[] = {
        0x5AU, 0xA5U, 0x06U, 0x82U,
        0x31U, 0x00U,  /* address 0x310 */
        0x5AU, 0xA5U,  /* D3:D2 = 0x5AA5 : start curve buffer write */
        0x01U, 0x00U   /* D1=1 data block, D0=0x00 (undefined) */
    };
    (void)OLED_SendRaw(trigger, sizeof(trigger));
}

static HAL_StatusTypeDef OLED_SendCurveSamples(const uint16_t *samples, uint8_t sample_count)
{
    uint8_t frame[6U + OLED_CURVE_CLEAR_SAMPLES * 2U];
    uint8_t i;
    HAL_StatusTypeDef status;

    if ((samples == NULL) || (sample_count == 0U) || (sample_count > OLED_CURVE_CLEAR_SAMPLES))
    {
        return HAL_ERROR;
    }

    /* Frame 1: write curve data to channel 0 buffer at 0x1000 */
    frame[0] = 0x5AU;
    frame[1] = 0xA5U;
    frame[2] = (uint8_t)(3U + sample_count * 2U);
    frame[3] = 0x82U;
    frame[4] = 0x10U;
    frame[5] = 0x00U;

    for (i = 0U; i < sample_count; i++)
    {
        frame[6U + i * 2U] = (uint8_t)(samples[i] >> 8);
        frame[7U + i * 2U] = (uint8_t)(samples[i] & 0xFFU);
    }

    status = OLED_SendRaw(frame, (uint16_t)(6U + sample_count * 2U));

    /* Frame 2: trigger the curve display refresh via 0x310 */
    OLED_TriggerCurveDisplay();

    return status;
}

static uint16_t OLED_ClampCurveSample(uint32_t raw_energy)
{
    if (raw_energy > OLED_CURVE_SAMPLE_MAX)
    {
        return OLED_CURVE_SAMPLE_MAX;
    }

    return (uint16_t)raw_energy;
}

static void OLED_ClearCurve(void)
{
    uint16_t zeros[OLED_CURVE_CLEAR_SAMPLES] = {0};
    (void)OLED_SendCurveSamples(zeros, OLED_CURVE_CLEAR_SAMPLES);
}

static void OLED_PushRadarSpectrum(const LD2410S_FrameInfo *info, uint8_t target_present)
{
    uint16_t samples[16];
    uint8_t i;

    (void)target_present;

    if (info == NULL)
    {
        return;
    }

    if (info->frame_type == LD2410S_FRAME_STANDARD)
    {
        for (i = 0; i < 16; i++)
        {
            samples[i] = OLED_ClampCurveSample(info->gate_energy[i]);
        }
    }
    else
    {
        memset(samples, 0, sizeof(samples));
    }

    (void)OLED_SendCurveSamples(samples, 16);
}

typedef enum
{
    DIST_ZONE_NONE = 0,
    DIST_ZONE_1 = 1,
    DIST_ZONE_2 = 2,
    DIST_ZONE_3 = 3,
    DIST_ZONE_4 = 4
} DistanceZone;

static void LED_SetByZone(DistanceZone zone)
{
    LED_AllOff();

    if (zone == DIST_ZONE_1) {
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    }
    else if (zone == DIST_ZONE_2) {
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    }
    else if (zone == DIST_ZONE_3) {
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    }
    else if (zone == DIST_ZONE_4) {
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
    }
}

static DistanceZone GetDistanceZone(uint16_t distance_cm, DistanceZone current_zone)
{
    if ((distance_cm == 0U) || (distance_cm > DISTANCE_MAX_CM))
    {
        return DIST_ZONE_NONE;
    }

    switch (current_zone)
    {
    case DIST_ZONE_1:
        if (distance_cm <= (30U + DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_1;
        if (distance_cm <= 50U) return DIST_ZONE_2;
        if (distance_cm <= 70U) return DIST_ZONE_3;
        return DIST_ZONE_4;

    case DIST_ZONE_2:
        if (distance_cm < (30U - DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_1;
        if (distance_cm <= (50U + DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_2;
        if (distance_cm <= 70U) return DIST_ZONE_3;
        return DIST_ZONE_4;

    case DIST_ZONE_3:
        if (distance_cm < (50U - DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_2;
        if (distance_cm <= (70U + DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_3;
        return DIST_ZONE_4;

    case DIST_ZONE_4:
        if (distance_cm < (70U - DISTANCE_HYSTERESIS_CM)) return DIST_ZONE_3;
        return DIST_ZONE_4;

    case DIST_ZONE_NONE:
    default:
        if (distance_cm <= 30U) return DIST_ZONE_1;
        if (distance_cm <= 50U) return DIST_ZONE_2;
        if (distance_cm <= 70U) return DIST_ZONE_3;
        return DIST_ZONE_4;
    }
}

static void LED_SetByDistance(uint16_t distance_cm)
{
    // 鍏堝叧闂墍鏈?LED
    LED_AllOff();
    
    // 鏍规嵁璺濈鍖洪棿鐐逛寒瀵瑰簲鐨?LED
    if (distance_cm > 0 && distance_cm <= 30) {
        // 0-0.3m LED1
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    }
    else if (distance_cm > 30 && distance_cm <= 50) {
        // 0.3-0.5m LED2
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    }
    else if (distance_cm > 50 && distance_cm <= 70) {
        // 0.5-0.7m LED3
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    }
    else if (distance_cm > 70 && distance_cm <= 100) {
        // 0.7-1.0m LED4
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
    }
    // 璺濈瓒呭嚭鑼冨洿鎴栨棤鏁堬紝鎵€鏈塋ED淇濇寔鐔勭伃
}

// 杞欢婊ゆ尝鍑芥暟锛氱Щ鍔ㄥ钩鍧囨护娉?
static uint16_t FilterDistance(uint16_t raw_distance)
{
    // 鏃犳晥璺濈鐩存帴杩斿洖0
    if (raw_distance == 0 || raw_distance > 200) {
        // 閲嶇疆婊ゆ尝鍣?
        filter_index = 0;
        filter_count = 0;
        filter_sum = 0;
        memset(filter_buffer, 0, sizeof(filter_buffer));
        return 0;
    }
    
    // 鍑忓幓鍗冲皢琚鐩栫殑鏃у€?
    filter_sum -= filter_buffer[filter_index];
    
    // 瀛樺叆鏂板€?
    filter_buffer[filter_index] = raw_distance;
    filter_sum += raw_distance;
    
    // 鏇存柊绱㈠紩
    filter_index++;
    if (filter_index >= FILTER_SIZE) {
        filter_index = 0;
    }
    
    // 鏇存柊璁℃暟锛堟渶澶氬埌 FILTER_SIZE锛?
    if (filter_count < FILTER_SIZE) {
        filter_count++;
    }
    
    // 杩斿洖骞冲潎鍊?
    return (uint16_t)(filter_sum / filter_count);
}

// 閲嶇疆婊ゆ尝鍣?
static void ResetFilter(void)
{
    filter_index = 0;
    filter_count = 0;
    filter_sum = 0;
    memset(filter_buffer, 0, sizeof(filter_buffer));
}

static uint16_t FilterDistanceStable(uint16_t raw_distance)
{
    if ((raw_distance == 0U) || (raw_distance > DISTANCE_FRAME_MAX_CM))
    {
        if (filter_count == 0U)
        {
            return 0;
        }
        return (uint16_t)(filter_sum / filter_count);
    }

    filter_sum -= filter_buffer[filter_index];
    filter_buffer[filter_index] = raw_distance;
    filter_sum += raw_distance;

    filter_index++;
    if (filter_index >= FILTER_SIZE)
    {
        filter_index = 0;
    }

    if (filter_count < FILTER_SIZE)
    {
        filter_count++;
    }

    return (uint16_t)(filter_sum / filter_count);
}

static void ResetRadarTracking(void)
{
    detect_confirm_count = 0;
    clear_confirm_count = 0;
    active_zone = DIST_ZONE_NONE;
    pending_zone = DIST_ZONE_NONE;
    pending_zone_count = 0;
    g_left_obstacle_present = 0;
    g_path_turn_right = 0;
    ResetFilter();
    LED_SetTestStatus(0, RadarGpioTargetPresent(), 0, 0);
}

static void UpdateZoneOutput(uint16_t filtered_distance)
{
    DistanceZone next_zone = GetDistanceZone(filtered_distance, (DistanceZone)active_zone);

    if (next_zone == DIST_ZONE_NONE)
    {
        return;
    }

    if (next_zone == active_zone)
    {
        pending_zone = DIST_ZONE_NONE;
        pending_zone_count = 0;
        return;
    }

    if (next_zone != (DistanceZone)pending_zone)
    {
        pending_zone = (uint8_t)next_zone;
        pending_zone_count = 1U;
    }
    else if (pending_zone_count < 255U)
    {
        pending_zone_count++;
    }

    if (pending_zone_count >= ZONE_CONFIRM_FRAMES)
    {
        active_zone = pending_zone;
        pending_zone = DIST_ZONE_NONE;
        pending_zone_count = 0;
    }
}

// 纭欢鑷锛氭墍鏈?LED 闂儊涓€娆?
static void LED_SelfTest(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
    HAL_Delay(500);
    LED_AllOff();
    HAL_Delay(500);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_UART4_Init();
    MX_USART3_UART_Init();
    MX_USART2_UART_Init();
    MX_USART6_UART_Init();

    /* USER CODE BEGIN 2 */
    LED_SelfTest();

    LD2410S_Init(&huart3, &huart6);
    LD2410S_StartReceiveIT();
    LD2410S_SwitchToStandardMode();

    g_last_frame_tick = HAL_GetTick();
    g_last_report_tick = HAL_GetTick();
    Debug_Printf("RADAR TEST START\r\n");
    Debug_Printf("RADAR UART: USART3 PB10=TX PB11=RX baud=%lu\r\n", (unsigned long)huart3.Init.BaudRate);
    Debug_Printf("RADAR GPIO: PB0 input, high=target_present\r\n");
    Debug_Printf("LED MAP: PC0=UART_FRESH PC1=PB0_TARGET PC2=LEFT_OBSTACLE PC3=TURN_RIGHT\r\n");
    Debug_Printf("OLED UART: USART2 PD5=TX PD6=RX baud=%lu CURVE_PTR=0x%04X CH=0\r\n",
                 (unsigned long)huart2.Init.BaudRate,
                 OLED_CURVE_DESC_PTR);
    Debug_Printf("THRESHOLD: left_present=%u-%ucm filter=%u confirm=%u stale=%ums\r\n",
                 LEFT_BLOCK_MIN_CM,
                 LEFT_BLOCK_MAX_CM,
                 FILTER_SIZE,
                 DETECT_CONFIRM_FRAMES,
                 RADAR_STALE_TIMEOUT_MS);
    OLED_ClearCurve();
    Debug_Printf("=== SCREEN TEST MODE: sending ramp pattern 0..900 to OLED ===\r\n");
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint32_t now = HAL_GetTick();
        uint8_t got_frame = 0U;
        uint8_t uart_fresh;
        uint8_t has_target;

        while (LD2410S_GetFrame(&g_frame_info))
        {
            got_frame = 1U;
            g_last_frame_tick = now;
            g_raw_distance_cm = g_frame_info.distance_cm;
            has_target = FrameHasTarget(&g_frame_info);

            if (has_target &&
                (g_frame_info.distance_cm > 0U) &&
                (g_frame_info.distance_cm <= DISTANCE_FRAME_MAX_CM))
            {
                g_filtered_distance_cm = FilterDistanceStable(g_frame_info.distance_cm);
                clear_confirm_count = 0;
                if (detect_confirm_count < 255U)
                {
                    detect_confirm_count++;
                }
                if ((detect_confirm_count >= DETECT_CONFIRM_FRAMES) && (g_filtered_distance_cm > 0U))
                {
                    UpdateZoneOutput(g_filtered_distance_cm);
                }
                g_left_obstacle_present = LeftObstacleRaw(1U, g_filtered_distance_cm);
            }
            else
            {
                g_filtered_distance_cm = 0U;
                g_left_obstacle_present = 0U;
                detect_confirm_count = 0;
                if (clear_confirm_count < 255U)
                {
                    clear_confirm_count++;
                }
                if (clear_confirm_count >= CLEAR_CONFIRM_FRAMES)
                {
                    ResetFilter();
                    active_zone = DIST_ZONE_NONE;
                    pending_zone = DIST_ZONE_NONE;
                    pending_zone_count = 0U;
                }
            }

            g_path_turn_right = g_left_obstacle_present;
        }

        uart_fresh = ((now - g_last_frame_tick) <= RADAR_STALE_TIMEOUT_MS) ? 1U : 0U;
        g_last_gpio_target = RadarGpioTargetPresent();
        if (!uart_fresh)
        {
            ResetRadarTracking();
        }
        else
        {
            LED_SetTestStatus(uart_fresh, g_last_gpio_target, g_left_obstacle_present, g_path_turn_right);
        }

        (void)got_frame;
        if ((now - g_last_report_tick) >= DEBUG_REPORT_PERIOD_MS)
        {
            uint8_t ei;
            Debug_Printf("RADAR FRAME: valid=%u type=%u target=%u state=%u raw_cm=%u filt_cm=%u zone=%u energy=",
                         uart_fresh,
                         g_frame_info.frame_type,
                         FrameHasTarget(&g_frame_info),
                         g_frame_info.target_state,
                         g_raw_distance_cm,
                         g_filtered_distance_cm,
                         active_zone);
            for (ei = 0; ei < 16; ei++)
            {
                Debug_Printf("%lu ", (unsigned long)g_frame_info.gate_energy[ei]);
            }
            Debug_Printf("\r\n");
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

        if ((now - g_last_oled_update_tick) >= OLED_UPDATE_PERIOD_MS)
        {
            /* --- screen test: send a ramp pattern, bypass radar --- */
            {
                uint16_t test_samples[16];
                uint8_t ti;
                for (ti = 0; ti < 16; ti++)
                {
                    test_samples[ti] = (uint16_t)(ti * 60U);
                }
                OLED_SendCurveSamples(test_samples, 16);
            }

            /* diagnostic: try reading back RAM at 0x1000 (0x83 command) */
            if ((now - g_last_report_tick) >= DEBUG_REPORT_PERIOD_MS)
            {
                uint8_t cmd[6] = {0x5A, 0xA5, 0x04, 0x83, 0x10, 0x00};
                uint8_t resp[8] = {0};
                HAL_StatusTypeDef tx_st, rx_st;

                tx_st = HAL_UART_Transmit(&huart2, cmd, 6, 20);
                memset(resp, 0, sizeof(resp));
                rx_st = HAL_UART_Receive(&huart2, resp, 6, 50);
                Debug_Printf("OLED: tx=%d rx=%d resp=%02X%02X%02X%02X%02X%02X\r\n",
                             (int)tx_st, (int)rx_st,
                             resp[0], resp[1], resp[2], resp[3], resp[4], resp[5]);
                g_last_report_tick = now;
            }
            g_last_oled_update_tick = now;
        }

        if (now - g_heartbeat_tick > 500)
        {
            g_heartbeat_tick = now;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        HAL_Delay(1);
        /* USER CODE END 3 */
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @br1ief USART2 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, MOT_L_DIR_Pin|MOT_R_DIR_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = LINE2_SIG1_Pin|LINE2_SIG2_Pin|LORA_AUX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RADAR_GPIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RADAR_GPIO_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = MOT_L_DIR_Pin|MOT_R_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LORA_LINK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LORA_LINK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LORA_WAKE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LORA_WAKE_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LORA_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LORA_RST_GPIO_Port, &GPIO_InitStruct);
  
  // 閰嶇疆 LED 寮曡剼涓鸿緭鍑?
  GPIO_InitStruct.Pin = LED1_Pin | LED2_Pin | LED3_Pin | LED4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
/*
 * The CubeMX project already defines HAL_UART_RxCpltCallback in stm32f4xx_it.c,
 * so do not define it again here.
 * Add this call inside the existing HAL_UART_RxCpltCallback instead:
 *   LD2410S_RxCpltCallback(huart);
 */
/* USER CODE END 4 */

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
}
#endif /* USE_FULL_ASSERT */

