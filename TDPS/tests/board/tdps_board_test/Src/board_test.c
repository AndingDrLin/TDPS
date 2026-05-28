#include "board_test.h"
#include "board_cli.h"
#include "board_uart_router.h"
#include "board_swd.h"
#include "board_motor.h"
#include "board_encoder.h"
#include "board_radar.h"
#include "board_lora.h"
#include "board_line.h"
#include "board_pins.h"

void BoardTest_Init(void)
{
    BoardMotor_Init();
    BoardEncoder_Init();
    BoardRadar_Init();
    BoardLora_Init();
    BoardLine_Init();
    BoardCli_Init();
    BoardSwd_Init();
    BoardUartRouter_Init();

    Board_Printf("\r\n%s %s\r\n", BOARD_FW_NAME, BOARD_FW_VERSION);
    Board_Printf("MCU STM32F407VET6, debug USART6 PC6/PC7 %lu 8N1\r\n", (unsigned long)BOARD_DEBUG_UART_BAUD);
    Board_Printf("Motor: LOCKED. Type 'help', or use Keil Watch variable g_board_swd.\r\n");
    BoardTest_PrintPins();
}

void BoardTest_Task(void)
{
    BoardCli_Task();
    BoardSwd_Task();
    BoardMotor_Task();
    BoardRadar_Task();
    BoardLora_Task();
    BoardLine_Task();
}

void BoardTest_PrintStatus(void)
{
    Board_Printf("UPTIME %lu ms SYSCLK=%lu HCLK=%lu PCLK1=%lu PCLK2=%lu\r\n",
                 (unsigned long)HAL_GetTick(),
                 (unsigned long)HAL_RCC_GetSysClockFreq(),
                 (unsigned long)HAL_RCC_GetHCLKFreq(),
                 (unsigned long)HAL_RCC_GetPCLK1Freq(),
                 (unsigned long)HAL_RCC_GetPCLK2Freq());
    BoardMotor_PrintStatus();
    BoardEncoder_Print();
    BoardRadar_PrintStatus();
    BoardLora_PrintStatus();
    BoardLine_PrintStatus();
}

void BoardTest_PrintPins(void)
{
    Board_Printf("PINS debug=USART6 PC6/PC7 radar=USART3 PB10/PB11 gpio=PE15 lora=UART5 PC12/PD2 link=PD0 wake=PD1 aux=PE0 rst=PE1\r\n");
    Board_Printf("PINS line=USART2 PA2/PA3 line_io=PB0/PB1 motorA=PC8 PWM PC9 DIR motorB=PB8 PWM PB9 DIR encA=PB4/PB5 encB=PB6/PB7\r\n");
}

void BoardTest_PrintGpio(void)
{
    Board_Printf("GPIO radar_PE15=%u lora_link_PD0=%u lora_wake_PD1=%u lora_aux_PE0=%u lora_rst_PE1=%u line_pb0=%u line_pb1=%u mot_a_dir_PC9=%u mot_b_dir_PB9=%u\r\n",
                 (unsigned)HAL_GPIO_ReadPin(RADAR_GPIO_GPIO_Port, RADAR_GPIO_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_LINK_GPIO_Port, LORA_LINK_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_WAKE_GPIO_Port, LORA_WAKE_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LORA_RST_GPIO_Port, LORA_RST_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG1_GPIO_Port, LINE2_SIG1_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LINE2_SIG2_GPIO_Port, LINE2_SIG2_Pin),
                 (unsigned)HAL_GPIO_ReadPin(MOT_A_DIR_GPIO_Port, MOT_A_DIR_Pin),
                 (unsigned)HAL_GPIO_ReadPin(MOT_B_DIR_GPIO_Port, MOT_B_DIR_Pin));
}

void BoardTest_SelfTestPassive(void)
{
    Board_Printf("SELFTEST passive begin\r\n");
    BoardTest_PrintStatus();
    BoardTest_PrintGpio();
    Board_Printf("SELFTEST passive end\r\n");
}

void BoardTest_SelfTestMotorSafe(void)
{
    if (BoardMotor_IsLocked())
    {
        Board_Printf("ERR motor locked, run 'motor unlock' first\r\n");
        return;
    }

    Board_Printf("SELFTEST motor-safe begin\r\n");
    (void)BoardMotor_Run(5, 0, 250);
    HAL_Delay(250);
    BoardMotor_StopAll();
    HAL_Delay(50);
    (void)BoardMotor_Run(0, 5, 250);
    HAL_Delay(250);
    BoardMotor_StopAll();
    BoardEncoder_Print();
    Board_Printf("SELFTEST motor-safe end\r\n");
}
