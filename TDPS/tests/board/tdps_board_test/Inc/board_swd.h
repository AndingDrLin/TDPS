#ifndef BOARD_SWD_H
#define BOARD_SWD_H

#include "main.h"

typedef enum
{
    BOARD_SWD_CMD_NONE = 0,
    BOARD_SWD_CMD_REFRESH = 1,
    BOARD_SWD_CMD_MOTOR_UNLOCK = 10,
    BOARD_SWD_CMD_MOTOR_LOCK = 11,
    BOARD_SWD_CMD_MOTOR_STOP = 12,
    BOARD_SWD_CMD_MOTOR_RUN = 13,
    BOARD_SWD_CMD_ENCODER_ZERO = 20,
    BOARD_SWD_CMD_LORA_RESET = 30,
    BOARD_SWD_CMD_LORA_WAKE = 31,
    BOARD_SWD_CMD_SELFTEST_PASSIVE = 40,
    BOARD_SWD_CMD_SELFTEST_MOTOR_SAFE = 41
} BoardSwdCommand;

typedef struct
{
    volatile uint32_t magic;
    volatile uint32_t command;
    volatile int32_t motor_a_duty;
    volatile int32_t motor_b_duty;
    volatile uint32_t motor_duration_ms;
    volatile uint32_t lora_wake_level;
    volatile uint32_t last_command;
    volatile uint32_t command_count;
    volatile uint32_t error_count;
    volatile uint32_t uptime_ms;
    volatile uint32_t sysclk_hz;
    volatile uint32_t motor_locked;
    volatile uint32_t motor_active;
    volatile int32_t encoder_a;
    volatile int32_t encoder_b;
    volatile uint32_t radar_gpio;
    volatile uint32_t radar_rx_count;
    volatile uint32_t radar_error_count;
    volatile uint32_t radar_target_state;
    volatile uint32_t radar_distance_cm;
    volatile uint32_t radar_last_frame_ms;
    volatile uint32_t lora_aux;
    volatile uint32_t lora_link;
    volatile uint32_t lora_wake;
    volatile uint32_t lora_rst;
    volatile uint32_t lora_rx_count;
    volatile uint32_t lora_error_count;
    volatile uint32_t line_sig1;
    volatile uint32_t line_sig2;
    volatile uint32_t line_rx_count;
    volatile uint32_t line_error_count;
    volatile uint32_t line_last_rx_ms;
    volatile int32_t motor_applied_a_duty;
    volatile int32_t motor_applied_b_duty;
    volatile uint32_t tim8_ccr3;
    volatile uint32_t tim10_ccr1;
    volatile uint32_t tim8_arr;
    volatile uint32_t tim10_arr;
} BoardSwdControl;

extern volatile BoardSwdControl g_board_swd;

void BoardSwd_Init(void);
void BoardSwd_Task(void);
void BoardSwd_Refresh(void);

#endif
