#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "lf_config.h"
#include "lf_control.h"
#include "lf_sensor.h"
#include "lf_sensor_uart.h"

void Error_Handler(void);

#define LINE_UART_TX_Pin        GPIO_PIN_2
#define LINE_UART_TX_GPIO_Port  GPIOA
#define LINE_UART_RX_Pin        GPIO_PIN_3
#define LINE_UART_RX_GPIO_Port  GPIOA

#define LINE_AUX1_Pin           GPIO_PIN_0
#define LINE_AUX1_GPIO_Port     GPIOB
#define LINE_AUX2_Pin           GPIO_PIN_1
#define LINE_AUX2_GPIO_Port     GPIOB
#define LINE_AUX_ACTIVE_HIGH    0U

#define LINE_STATUS_LED_Pin       GPIO_PIN_13
#define LINE_STATUS_LED_GPIO_Port GPIOC
#define LINE_STATUS_LED_ON_LEVEL  GPIO_PIN_RESET

typedef struct {
    uint32_t tick_ms;
    uint32_t loop_count;
    uint32_t sample_count;
    uint32_t last_update_ms;
    uint8_t initialized;
    uint8_t fault_flags;

    uint32_t rx_byte_count;
    uint32_t analog_frame_count;
    uint32_t digital_frame_count;
    uint32_t parse_error_count;
    uint32_t uart_error_count;
    uint32_t last_frame_age_ms;
    uint8_t last_rx_byte;
    char last_ascii_frame[LF_SENSOR_UART_MAX_FRAME_LEN];

    uint16_t uart_analog[LF_SENSOR_COUNT];
    uint8_t uart_digital[LF_SENSOR_COUNT];
    uint16_t main_raw[LF_SENSOR_COUNT];

    uint8_t aux_gpio_level[2];
    uint8_t aux_active[2];
    uint8_t aux_level_mask;
    uint8_t aux_active_mask;

    uint16_t norm[LF_SENSOR_COUNT];
    uint16_t filtered_u16[LF_SENSOR_COUNT];
    uint32_t signal_sum;
    uint16_t peak_value;
    uint16_t contrast_value;
    uint8_t peak_index;
    uint8_t active_count;
    float line_confidence;
    int8_t edge_hint;
    int32_t position;
    uint8_t line_detected;

    float error;
    float dt_s;
    int16_t correction;
    int16_t base_speed;
    int16_t motor_left_raw;
    int16_t motor_right_raw;
    int16_t motor_left_applied;
    int16_t motor_right_applied;
    uint8_t motor_command_valid;

    uint8_t sensor_input_mode;
    uint8_t sensor_invert_polarity;
    uint8_t sensor_digital_active_high;
    uint16_t sensor_filter_alpha_x100;
    uint16_t line_detect_min_sum;
    uint16_t line_detect_min_peak;
    uint16_t line_detect_min_contrast;
    uint8_t calibration_status;
    uint8_t calibration_done;
} LineSensorUartWatchInfo;

extern UART_HandleTypeDef huart2;
extern volatile uint8_t g_line_uart_rx_byte;
extern volatile LineSensorUartWatchInfo g_line_watch;
extern LF_SensorFrame g_line_frame;
extern LF_PIDState g_line_pid;
extern volatile int16_t g_line_motor_left_applied;
extern volatile int16_t g_line_motor_right_applied;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
