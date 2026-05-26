#ifdef WL_USE_POSIX_SERIAL_PORT

#include "wl_config.h"
#include "wl_platform.h"
#include "wl_port_posix_serial.h"
#include "wl_protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *port_path;
    uint32_t baudrate;
    uint32_t checkpoint_id;
    uint32_t duration_ms;
    WL_PosixAuxMode aux_mode;
} CliOptions;

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s --port <device> [options]\n"
            "\n"
            "options:\n"
            "  --port <device>          Serial device, e.g. /dev/cu.usbserial-XXXX\n"
            "  --baud <rate>            Serial baud rate, default %u\n"
            "  --checkpoint <id>        Checkpoint id, default %u\n"
            "  --duration <MM:SS>       Race duration to transmit, default 00:00\n"
            "  --aux-mode <mode>        always-ready or cts, default always-ready\n"
            "  --help                   Show this help\n",
            program,
            (unsigned)g_wl_config.uart_baudrate,
            (unsigned)g_wl_config.checkpoint_arch_2_1);
}

static bool parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || text[0] == '\0' || out == NULL) {
        return false;
    }

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool parse_aux_mode(const char *text, WL_PosixAuxMode *out)
{
    if (text == NULL || out == NULL) {
        return false;
    }

    if (strcmp(text, "always-ready") == 0) {
        *out = WL_POSIX_AUX_ALWAYS_READY;
        return true;
    }

    if (strcmp(text, "cts") == 0) {
        *out = WL_POSIX_AUX_CTS;
        return true;
    }

    return false;
}

static bool parse_duration(const char *text, uint32_t *duration_ms)
{
    const char *colon;
    char minutes_text[16];
    uint32_t minutes;
    uint32_t seconds;
    size_t minutes_len;

    if (text == NULL || duration_ms == NULL) {
        return false;
    }

    colon = strchr(text, ':');
    if (colon == NULL || strchr(colon + 1, ':') != NULL) {
        return false;
    }

    minutes_len = (size_t)(colon - text);
    if (minutes_len == 0U || minutes_len >= sizeof(minutes_text)) {
        return false;
    }

    memcpy(minutes_text, text, minutes_len);
    minutes_text[minutes_len] = '\0';

    if (!parse_u32(minutes_text, &minutes) || !parse_u32(colon + 1, &seconds)) {
        return false;
    }

    if (seconds >= 60U || minutes > ((UINT32_MAX / 1000U) / 60U)) {
        return false;
    }

    *duration_ms = ((minutes * 60U) + seconds) * 1000U;
    return true;
}

static bool parse_args(int argc, char **argv, CliOptions *options)
{
    int i;

    if (options == NULL) {
        return false;
    }

    options->port_path = NULL;
    options->baudrate = g_wl_config.uart_baudrate;
    options->checkpoint_id = g_wl_config.checkpoint_arch_2_1;
    options->duration_ms = 0U;
    options->aux_mode = WL_POSIX_AUX_ALWAYS_READY;

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }

        if (strcmp(arg, "--port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "error: --port requires a value\n");
                return false;
            }
            options->port_path = argv[i];
            continue;
        }

        if (strcmp(arg, "--baud") == 0) {
            if (++i >= argc || !parse_u32(argv[i], &options->baudrate)) {
                fprintf(stderr, "error: --baud requires an unsigned integer\n");
                return false;
            }
            continue;
        }

        if (strcmp(arg, "--checkpoint") == 0) {
            if (++i >= argc || !parse_u32(argv[i], &options->checkpoint_id)) {
                fprintf(stderr, "error: --checkpoint requires an unsigned integer\n");
                return false;
            }
            continue;
        }

        if (strcmp(arg, "--duration") == 0) {
            if (++i >= argc || !parse_duration(argv[i], &options->duration_ms)) {
                fprintf(stderr, "error: --duration must use MM:SS with seconds below 60\n");
                return false;
            }
            continue;
        }

        if (strcmp(arg, "--aux-mode") == 0) {
            if (++i >= argc || !parse_aux_mode(argv[i], &options->aux_mode)) {
                fprintf(stderr, "error: --aux-mode must be always-ready or cts\n");
                return false;
            }
            continue;
        }

        fprintf(stderr, "error: unknown argument: %s\n", arg);
        return false;
    }

    if (options->port_path == NULL) {
        fprintf(stderr, "error: --port is required\n");
        return false;
    }

    return true;
}

static int send_checkpoint_payload(const CliOptions *options)
{
    char msg[WL_MSG_MAX_LEN];
    uint16_t len;

    if (WL_PosixSerial_Configure(options->port_path,
                                 options->baudrate,
                                 options->aux_mode) != 0) {
        fprintf(stderr, "error: %s\n", WL_PosixSerial_GetLastError());
        return 1;
    }

    WL_Platform_Init();
    if (WL_PosixSerial_HadIoError()) {
        fprintf(stderr, "error: %s\n", WL_PosixSerial_GetLastError());
        WL_PosixSerial_Close();
        return 1;
    }

    len = WL_Protocol_BuildCheckpointMsg(msg,
                                         (uint16_t)sizeof(msg),
                                         options->checkpoint_id,
                                         options->duration_ms);
    if (len == 0U) {
        fprintf(stderr, "error: failed to build checkpoint message\n");
        WL_PosixSerial_Close();
        return 1;
    }

    printf("[pc-serial] TX: %s", msg);
    WL_Platform_UART_Send((const uint8_t *)msg, len);
    if (WL_PosixSerial_HadIoError()) {
        fprintf(stderr, "error: %s\n", WL_PosixSerial_GetLastError());
        WL_PosixSerial_Close();
        return 1;
    }

    WL_Platform_DelayMs(100U);
    WL_PosixSerial_Close();
    return 0;
}

int main(int argc, char **argv)
{
    CliOptions options;

    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    return send_checkpoint_payload(&options);
}

#endif
