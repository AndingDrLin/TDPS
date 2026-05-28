#include "board_swd.h"
#include "board_motor.h"
#include "board_encoder.h"
#include "board_radar.h"
#include "board_lora.h"
#include "board_line.h"
#include "board_test.h"

#define BOARD_SWD_MAGIC 0x54445053UL

__attribute__((used)) volatile BoardSwdControl g_board_swd;

void BoardSwd_Refresh(void)
{
    g_board_swd.uptime_ms = HAL_GetTick();
    g_board_swd.sysclk_hz = HAL_RCC_GetSysClockFreq();
    g_board_swd.motor_locked = BoardMotor_IsLocked();
    g_board_swd.motor_active = BoardMotor_IsActive();
    g_board_swd.encoder_a = BoardEncoder_ReadA();
    g_board_swd.encoder_b = BoardEncoder_ReadB();
    g_board_swd.radar_gpio = HAL_GPIO_ReadPin(RADAR_GPIO_GPIO_Port, RADAR_GPIO_Pin);
    g_board_swd.radar_rx_count = BoardRadar_GetRxCount();
    g_board_swd.radar_error_count = BoardRadar_GetErrorCount();
    g_board_swd.radar_target_state = BoardRadar_GetTargetState();
    g_board_swd.radar_distance_cm = BoardRadar_GetDistanceCm();
    g_board_swd.radar_last_frame_ms = BoardRadar_GetLastFrameTick();
    g_board_swd.lora_aux = HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin);
    g_board_swd.lora_link = HAL_GPIO_ReadPin(LORA_LINK_GPIO_Port, LORA_LINK_Pin);
    g_board_swd.lora_wake = HAL_GPIO_ReadPin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin);
    g_board_swd.lora_rst = HAL_GPIO_ReadPin(LORA_RST_GPIO_Port, LORA_RST_Pin);
    g_board_swd.lora_rx_count = BoardLora_GetRxCount();
    g_board_swd.lora_error_count = BoardLora_GetErrorCount();
    g_board_swd.line_sig1 = HAL_GPIO_ReadPin(LINE2_SIG1_GPIO_Port, LINE2_SIG1_Pin);
    g_board_swd.line_sig2 = HAL_GPIO_ReadPin(LINE2_SIG2_GPIO_Port, LINE2_SIG2_Pin);
    g_board_swd.line_rx_count = BoardLine_GetRxCount();
    g_board_swd.line_error_count = BoardLine_GetErrorCount();
    g_board_swd.line_last_rx_ms = BoardLine_GetLastRxTick();
    g_board_swd.motor_applied_a_duty = BoardMotor_GetDutyA();
    g_board_swd.motor_applied_b_duty = BoardMotor_GetDutyB();
    g_board_swd.tim8_ccr3 = TIM8->CCR3;
    g_board_swd.tim10_ccr1 = TIM10->CCR1;
    g_board_swd.tim8_arr = TIM8->ARR;
    g_board_swd.tim10_arr = TIM10->ARR;
}

void BoardSwd_Init(void)
{
    g_board_swd.magic = BOARD_SWD_MAGIC;
    g_board_swd.command = BOARD_SWD_CMD_NONE;
    g_board_swd.motor_a_duty = 0;
    g_board_swd.motor_b_duty = 0;
    g_board_swd.motor_duration_ms = 300;
    g_board_swd.lora_wake_level = 0;
    g_board_swd.last_command = BOARD_SWD_CMD_NONE;
    g_board_swd.command_count = 0;
    g_board_swd.error_count = 0;
    BoardSwd_Refresh();
}

void BoardSwd_Task(void)
{
    uint32_t command = g_board_swd.command;

    BoardSwd_Refresh();

    if (command == BOARD_SWD_CMD_NONE)
    {
        return;
    }

    g_board_swd.command = BOARD_SWD_CMD_NONE;
    g_board_swd.last_command = command;
    g_board_swd.command_count++;

    switch ((BoardSwdCommand)command)
    {
    case BOARD_SWD_CMD_REFRESH:
        break;
    case BOARD_SWD_CMD_MOTOR_UNLOCK:
        BoardMotor_Unlock();
        break;
    case BOARD_SWD_CMD_MOTOR_LOCK:
        BoardMotor_Lock();
        break;
    case BOARD_SWD_CMD_MOTOR_STOP:
        BoardMotor_StopAll();
        break;
    case BOARD_SWD_CMD_MOTOR_RUN:
        if (BoardMotor_Run((int)g_board_swd.motor_a_duty,
                           (int)g_board_swd.motor_b_duty,
                           g_board_swd.motor_duration_ms) != HAL_OK)
        {
            g_board_swd.error_count++;
        }
        break;
    case BOARD_SWD_CMD_ENCODER_ZERO:
        BoardEncoder_Zero();
        break;
    case BOARD_SWD_CMD_LORA_RESET:
        if (BoardMotor_IsActive())
        {
            g_board_swd.error_count++;
        }
        else
        {
            BoardLora_Reset();
        }
        break;
    case BOARD_SWD_CMD_LORA_WAKE:
        BoardLora_SetWake((uint8_t)g_board_swd.lora_wake_level);
        break;
    case BOARD_SWD_CMD_SELFTEST_PASSIVE:
        BoardTest_SelfTestPassive();
        break;
    case BOARD_SWD_CMD_SELFTEST_MOTOR_SAFE:
        BoardTest_SelfTestMotorSafe();
        break;
    default:
        g_board_swd.error_count++;
        break;
    }

    BoardSwd_Refresh();
}
