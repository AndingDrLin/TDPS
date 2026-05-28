#include "board_cli.h"
#include "board_test.h"
#include "board_motor.h"
#include "board_encoder.h"
#include "board_radar.h"
#include "board_lora.h"
#include "board_line.h"
#include "board_pins.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CLI_LINE_SIZE 128U
#define CLI_ARG_MAX 8U

extern UART_HandleTypeDef huart6;

static char s_line[CLI_LINE_SIZE];
static volatile uint16_t s_write;
static volatile uint8_t s_ready;
static volatile uint32_t s_error_count;

HAL_StatusTypeDef Board_Write(const uint8_t *data, uint16_t len)
{
    if (data == 0 || len == 0U)
    {
        return HAL_OK;
    }
    return HAL_UART_Transmit(&huart6, (uint8_t *)data, len, 200);
}

int Board_Printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return len;
    }
    if (len >= (int)sizeof(buf))
    {
        len = (int)sizeof(buf) - 1;
    }
    (void)Board_Write((uint8_t *)buf, (uint16_t)len);
    return len;
}

void BoardCli_Init(void)
{
    s_write = 0;
    s_ready = 0;
    s_error_count = 0;
}

void BoardCli_OnRxByte(uint8_t byte)
{
    if (byte == '\r' || byte == '\n')
    {
        if (s_write > 0U)
        {
            s_line[s_write] = '\0';
            s_ready = 1;
        }
        return;
    }

    if (byte == 0x08U || byte == 0x7FU)
    {
        if (s_write > 0U)
        {
            s_write--;
        }
        return;
    }

    if (s_ready)
    {
        return;
    }

    if (s_write < (CLI_LINE_SIZE - 1U))
    {
        s_line[s_write++] = (char)byte;
    }
}

void BoardCli_OnError(void)
{
    s_error_count++;
}

static int parse_args(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max_args)
    {
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t')
        {
            p++;
        }
        if (*p != '\0')
        {
            *p++ = '\0';
        }
    }

    return argc;
}

static int arg_int(const char *text)
{
    return atoi(text);
}

static uint32_t arg_u32(const char *text)
{
    return (uint32_t)strtoul(text, 0, 10);
}

static uint8_t is_on(const char *text)
{
    return (strcmp(text, "on") == 0 || strcmp(text, "1") == 0) ? 1U : 0U;
}

static void print_help(void)
{
    Board_Printf("Commands:\r\n");
    Board_Printf("  help version status pins uptime reboot\r\n");
    Board_Printf("  gpio read all\r\n");
    Board_Printf("  radar status|gpio|raw on|raw off|parse on|parse off\r\n");
    Board_Printf("  lora status|reset|wake 0|wake 1|raw on|raw off|send <text>\r\n");
    Board_Printf("  line status|io|raw on|raw off|send <text>\r\n");
    Board_Printf("  enc read|zero|watch <ms>\r\n");
    Board_Printf("  motor status|unlock|lock|stop|max <pct>|a <duty> <ms>|b <duty> <ms>|both <a> <b> <ms>\r\n");
    Board_Printf("  selftest passive|motor-safe\r\n");
}

static void command_motor(int argc, char *argv[])
{
    if (argc < 2)
    {
        BoardMotor_PrintStatus();
        return;
    }
    if (strcmp(argv[1], "status") == 0)
    {
        BoardMotor_PrintStatus();
    }
    else if (strcmp(argv[1], "unlock") == 0)
    {
        BoardMotor_Unlock();
        Board_Printf("Motor unlocked\r\n");
    }
    else if (strcmp(argv[1], "lock") == 0)
    {
        BoardMotor_Lock();
        Board_Printf("Motor locked\r\n");
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        BoardMotor_StopAll();
        Board_Printf("Motor stopped\r\n");
    }
    else if (strcmp(argv[1], "max") == 0 && argc >= 3)
    {
        BoardMotor_SetMaxDuty(arg_int(argv[2]));
        Board_Printf("Motor max=%d%%\r\n", BoardMotor_GetMaxDuty());
    }
    else if (strcmp(argv[1], "a") == 0 && argc >= 4)
    {
        (void)BoardMotor_Run(arg_int(argv[2]), 0, arg_u32(argv[3]));
    }
    else if (strcmp(argv[1], "b") == 0 && argc >= 4)
    {
        (void)BoardMotor_Run(0, arg_int(argv[2]), arg_u32(argv[3]));
    }
    else if (strcmp(argv[1], "both") == 0 && argc >= 5)
    {
        (void)BoardMotor_Run(arg_int(argv[2]), arg_int(argv[3]), arg_u32(argv[4]));
    }
    else
    {
        Board_Printf("ERR motor command\r\n");
    }
}

static void command_radar(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "status") == 0)
    {
        BoardRadar_PrintStatus();
    }
    else if (strcmp(argv[1], "gpio") == 0)
    {
        BoardRadar_PrintGpio();
    }
    else if (strcmp(argv[1], "raw") == 0 && argc >= 3)
    {
        BoardRadar_SetRaw(is_on(argv[2]));
    }
    else if (strcmp(argv[1], "parse") == 0 && argc >= 3)
    {
        BoardRadar_SetParse(is_on(argv[2]));
    }
    else
    {
        Board_Printf("ERR radar command\r\n");
    }
}

static void command_lora(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "status") == 0)
    {
        BoardLora_PrintStatus();
    }
    else if (strcmp(argv[1], "reset") == 0)
    {
        if (BoardMotor_IsActive())
        {
            Board_Printf("ERR stop motor before lora reset\r\n");
            return;
        }
        BoardLora_Reset();
    }
    else if (strcmp(argv[1], "wake") == 0 && argc >= 3)
    {
        BoardLora_SetWake((uint8_t)arg_int(argv[2]));
    }
    else if (strcmp(argv[1], "raw") == 0 && argc >= 3)
    {
        BoardLora_SetRaw(is_on(argv[2]));
    }
    else if (strcmp(argv[1], "send") == 0 && argc >= 3)
    {
        (void)BoardLora_SendString(argv[2]);
        (void)BoardLora_SendString("\r\n");
    }
    else
    {
        Board_Printf("ERR lora command\r\n");
    }
}

static void command_line(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "status") == 0)
    {
        BoardLine_PrintStatus();
    }
    else if (strcmp(argv[1], "io") == 0)
    {
        BoardLine_PrintIo();
    }
    else if (strcmp(argv[1], "raw") == 0 && argc >= 3)
    {
        BoardLine_SetRaw(is_on(argv[2]));
    }
    else if (strcmp(argv[1], "send") == 0 && argc >= 3)
    {
        (void)BoardLine_SendString(argv[2]);
        (void)BoardLine_SendString("\r\n");
    }
    else
    {
        Board_Printf("ERR line command\r\n");
    }
}

static void command_enc(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "read") == 0)
    {
        BoardEncoder_Print();
    }
    else if (strcmp(argv[1], "zero") == 0)
    {
        BoardEncoder_Zero();
        Board_Printf("ENC zeroed\r\n");
    }
    else if (strcmp(argv[1], "watch") == 0 && argc >= 3)
    {
        if (BoardMotor_IsActive())
        {
            Board_Printf("ERR stop motor before enc watch\r\n");
            return;
        }
        BoardEncoder_Watch(arg_u32(argv[2]));
    }
    else
    {
        Board_Printf("ERR enc command\r\n");
    }
}

static void execute_line(char *line)
{
    char *argv[CLI_ARG_MAX];
    int argc = parse_args(line, argv, CLI_ARG_MAX);

    if (argc == 0)
    {
        return;
    }

    if (strcmp(argv[0], "help") == 0)
    {
        print_help();
    }
    else if (strcmp(argv[0], "version") == 0)
    {
        Board_Printf("%s %s build %s %s\r\n", BOARD_FW_NAME, BOARD_FW_VERSION, __DATE__, __TIME__);
    }
    else if (strcmp(argv[0], "status") == 0)
    {
        BoardTest_PrintStatus();
    }
    else if (strcmp(argv[0], "pins") == 0)
    {
        BoardTest_PrintPins();
    }
    else if (strcmp(argv[0], "uptime") == 0)
    {
        Board_Printf("%lu ms\r\n", (unsigned long)HAL_GetTick());
    }
    else if (strcmp(argv[0], "reboot") == 0)
    {
        BoardMotor_StopAll();
        Board_Printf("Rebooting\r\n");
        HAL_Delay(20);
        NVIC_SystemReset();
    }
    else if (strcmp(argv[0], "stop") == 0)
    {
        BoardMotor_StopAll();
        Board_Printf("Motor stopped\r\n");
    }
    else if (strcmp(argv[0], "gpio") == 0)
    {
        BoardTest_PrintGpio();
    }
    else if (strcmp(argv[0], "radar") == 0)
    {
        command_radar(argc, argv);
    }
    else if (strcmp(argv[0], "lora") == 0)
    {
        command_lora(argc, argv);
    }
    else if (strcmp(argv[0], "line") == 0)
    {
        command_line(argc, argv);
    }
    else if (strcmp(argv[0], "enc") == 0)
    {
        command_enc(argc, argv);
    }
    else if (strcmp(argv[0], "motor") == 0)
    {
        command_motor(argc, argv);
    }
    else if (strcmp(argv[0], "selftest") == 0 && argc >= 2 && strcmp(argv[1], "passive") == 0)
    {
        BoardTest_SelfTestPassive();
    }
    else if (strcmp(argv[0], "selftest") == 0 && argc >= 2 && strcmp(argv[1], "motor-safe") == 0)
    {
        BoardTest_SelfTestMotorSafe();
    }
    else
    {
        Board_Printf("ERR unknown command '%s'\r\n", argv[0]);
    }
}

void BoardCli_Task(void)
{
    char local[CLI_LINE_SIZE];

    if (!s_ready)
    {
        return;
    }

    __disable_irq();
    strncpy(local, s_line, sizeof(local));
    local[sizeof(local) - 1U] = '\0';
    s_write = 0;
    s_ready = 0;
    __enable_irq();

    Board_Printf("> %s\r\n", local);
    execute_line(local);
    Board_Printf("> ");
}
