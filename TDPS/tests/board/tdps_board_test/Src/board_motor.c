#include "board_motor.h"
#include "board_cli.h"
#include "board_pins.h"

extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim10;

static uint8_t s_locked = 1;
static int s_max_duty = BOARD_MOTOR_DEFAULT_MAX_DUTY;
static int s_duty_a;
static int s_duty_b;
static uint32_t s_stop_tick;
static uint8_t s_active;

static int abs_int(int value)
{
    return value < 0 ? -value : value;
}

static uint32_t duty_to_pulse(TIM_HandleTypeDef *htim, int duty)
{
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim) + 1U;
    return (period * (uint32_t)abs_int(duty)) / 100U;
}

static void apply_motor_a(int duty)
{
    HAL_GPIO_WritePin(MOT_A_DIR_GPIO_Port, MOT_A_DIR_Pin, duty >= 0 ? BOARD_MOTOR_A_FORWARD_STATE : BOARD_MOTOR_A_REVERSE_STATE);
    htim8.Instance->CCR3 = duty_to_pulse(&htim8, duty);
}

static void apply_motor_b(int duty)
{
    HAL_GPIO_WritePin(MOT_B_DIR_GPIO_Port, MOT_B_DIR_Pin, duty >= 0 ? BOARD_MOTOR_B_FORWARD_STATE : BOARD_MOTOR_B_REVERSE_STATE);
    htim10.Instance->CCR1 = duty_to_pulse(&htim10, duty);
}

void BoardMotor_Init(void)
{
    (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
    (void)HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1);
    BoardMotor_StopAll();
    s_locked = 1;
}

void BoardMotor_Task(void)
{
    if (s_active && ((int32_t)(HAL_GetTick() - s_stop_tick) >= 0))
    {
        BoardMotor_StopAll();
        Board_Printf("Motor timeout stop\r\n");
    }
}

void BoardMotor_StopAll(void)
{
    s_duty_a = 0;
    s_duty_b = 0;
    s_active = 0;
    apply_motor_a(0);
    apply_motor_b(0);
}

void BoardMotor_Lock(void)
{
    BoardMotor_StopAll();
    s_locked = 1;
}

void BoardMotor_Unlock(void)
{
    s_locked = 0;
}

uint8_t BoardMotor_IsLocked(void)
{
    return s_locked;
}

uint8_t BoardMotor_IsActive(void)
{
    return s_active;
}

void BoardMotor_SetMaxDuty(int max_duty)
{
    if (max_duty < 1)
    {
        max_duty = 1;
    }
    if (max_duty > 80)
    {
        max_duty = 80;
    }
    s_max_duty = max_duty;
}

int BoardMotor_GetMaxDuty(void)
{
    return s_max_duty;
}

int BoardMotor_GetDutyA(void)
{
    return s_duty_a;
}

int BoardMotor_GetDutyB(void)
{
    return s_duty_b;
}

HAL_StatusTypeDef BoardMotor_Run(int duty_a, int duty_b, uint32_t duration_ms)
{
    if (s_locked)
    {
        Board_Printf("ERR motor locked, run 'motor unlock' first\r\n");
        return HAL_ERROR;
    }
    if (duration_ms == 0U || duration_ms > BOARD_MOTOR_DEFAULT_MAX_MS)
    {
        Board_Printf("ERR duration must be 1..%lu ms\r\n", (unsigned long)BOARD_MOTOR_DEFAULT_MAX_MS);
        return HAL_ERROR;
    }
    if ((abs_int(duty_a) > s_max_duty) || (abs_int(duty_b) > s_max_duty))
    {
        Board_Printf("ERR duty exceeds max %d%%\r\n", s_max_duty);
        return HAL_ERROR;
    }

    s_duty_a = duty_a;
    s_duty_b = duty_b;
    apply_motor_a(duty_a);
    apply_motor_b(duty_b);
    s_stop_tick = HAL_GetTick() + duration_ms;
    s_active = (duty_a != 0 || duty_b != 0) ? 1U : 0U;

    Board_Printf("Motor run A=%d%% B=%d%% duration=%lu ms\r\n", duty_a, duty_b, (unsigned long)duration_ms);
    return HAL_OK;
}

void BoardMotor_PrintStatus(void)
{
    Board_Printf("MOTOR locked=%u active=%u max=%d%% A=%d%% B=%d%% pins A_PWM=PC8/TIM8_CH3 A_DIR=PC9 B_PWM=PB8/TIM10_CH1 B_DIR=PB9\r\n",
                 (unsigned)s_locked,
                 (unsigned)s_active,
                 s_max_duty,
                 s_duty_a,
                 s_duty_b);
}
