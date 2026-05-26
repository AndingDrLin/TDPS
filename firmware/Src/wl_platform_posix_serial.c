#ifdef WL_USE_POSIX_SERIAL_PORT

#include "wl_config.h"
#include "wl_platform.h"
#include "wl_port_posix_serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static int s_serial_fd = -1;
static WL_PosixAuxMode s_aux_mode = WL_POSIX_AUX_ALWAYS_READY;
static uint64_t s_start_ms = 0U;
static bool s_has_original_termios = false;
static struct termios s_original_termios;
static bool s_io_error = false;
static char s_last_error[160];

static uint64_t host_now_ms(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return 0U;
    }

    return ((uint64_t)tv.tv_sec * 1000U) + ((uint64_t)tv.tv_usec / 1000U);
}

static void clear_error(void)
{
    s_io_error = false;
    s_last_error[0] = '\0';
}

static void set_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    (void)vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
    s_io_error = true;
}

static bool baud_to_speed(uint32_t baudrate, speed_t *speed)
{
    if (speed == NULL) {
        return false;
    }

    switch (baudrate) {
    case 9600U:
        *speed = B9600;
        return true;
    case 19200U:
        *speed = B19200;
        return true;
    case 38400U:
        *speed = B38400;
        return true;
    case 57600U:
        *speed = B57600;
        return true;
#ifdef B115200
    case 115200U:
        *speed = B115200;
        return true;
#endif
    default:
        return false;
    }
}

static bool wait_fd_writable(uint32_t timeout_ms)
{
    fd_set writefds;
    struct timeval timeout;
    int rc;

    if (s_serial_fd < 0) {
        set_error("serial port is not open");
        return false;
    }

    FD_ZERO(&writefds);
    FD_SET(s_serial_fd, &writefds);
    timeout.tv_sec = (time_t)(timeout_ms / 1000U);
    timeout.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);

    do {
        rc = select(s_serial_fd + 1, NULL, &writefds, NULL, &timeout);
    } while (rc < 0 && errno == EINTR);

    if (rc > 0) {
        return true;
    }

    if (rc < 0) {
        set_error("serial write wait failed: %s", strerror(errno));
    }

    return false;
}

static void sleep_ms(uint32_t ms)
{
    while (ms > 0U) {
        uint32_t chunk_ms = ms > 1000U ? 1000U : ms;
        struct timeval timeout;
        int rc;

        timeout.tv_sec = (time_t)(chunk_ms / 1000U);
        timeout.tv_usec = (suseconds_t)((chunk_ms % 1000U) * 1000U);

        do {
            rc = select(0, NULL, NULL, NULL, &timeout);
        } while (rc < 0 && errno == EINTR);

        if (rc < 0) {
            return;
        }

        ms -= chunk_ms;
    }
}

static int configure_termios(uint32_t baudrate)
{
    struct termios options;
    speed_t speed;

    if (!baud_to_speed(baudrate, &speed)) {
        set_error("unsupported baud rate: %u", (unsigned)baudrate);
        return -1;
    }

    if (tcgetattr(s_serial_fd, &s_original_termios) != 0) {
        set_error("tcgetattr failed: %s", strerror(errno));
        return -1;
    }
    s_has_original_termios = true;

    options = s_original_termios;
    options.c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    options.c_oflag &= (tcflag_t)~OPOST;
    options.c_lflag &= (tcflag_t)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    options.c_cflag &= (tcflag_t)~(CSIZE | PARENB | CSTOPB);
#ifdef CRTSCTS
    options.c_cflag &= (tcflag_t)~CRTSCTS;
#endif
#ifdef CCTS_OFLOW
    options.c_cflag &= (tcflag_t)~CCTS_OFLOW;
#endif
#ifdef CRTS_IFLOW
    options.c_cflag &= (tcflag_t)~CRTS_IFLOW;
#endif
    options.c_cflag |= (CS8 | CLOCAL | CREAD);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0) {
        set_error("failed to set baud rate: %s", strerror(errno));
        return -1;
    }

    if (tcsetattr(s_serial_fd, TCSANOW, &options) != 0) {
        set_error("tcsetattr failed: %s", strerror(errno));
        return -1;
    }

    (void)tcflush(s_serial_fd, TCIOFLUSH);
    return 0;
}

int WL_PosixSerial_Configure(const char *port_path,
                             uint32_t baudrate,
                             WL_PosixAuxMode aux_mode)
{
    int flags;

    WL_PosixSerial_Close();
    clear_error();

    if (port_path == NULL || port_path[0] == '\0') {
        set_error("serial port path is required");
        return -1;
    }

#if !defined(TIOCM_CTS)
    if (aux_mode == WL_POSIX_AUX_CTS) {
        set_error("CTS AUX mode is not supported on this platform");
        return -1;
    }
#endif

    s_serial_fd = open(port_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s_serial_fd < 0) {
        set_error("cannot open serial port %s: %s", port_path, strerror(errno));
        return -1;
    }

    if (configure_termios(baudrate) != 0) {
        WL_PosixSerial_Close();
        return -1;
    }

    flags = fcntl(s_serial_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(s_serial_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        set_error("failed to enable nonblocking serial I/O: %s", strerror(errno));
        WL_PosixSerial_Close();
        return -1;
    }

    s_aux_mode = aux_mode;
    s_start_ms = host_now_ms();
    return 0;
}

void WL_PosixSerial_Close(void)
{
    if (s_serial_fd >= 0) {
        if (s_has_original_termios) {
            (void)tcsetattr(s_serial_fd, TCSANOW, &s_original_termios);
        }
        (void)close(s_serial_fd);
    }

    s_serial_fd = -1;
    s_has_original_termios = false;
}

bool WL_PosixSerial_HadIoError(void)
{
    return s_io_error;
}

const char *WL_PosixSerial_GetLastError(void)
{
    if (s_last_error[0] == '\0') {
        return "no serial error";
    }

    return s_last_error;
}

void WL_Platform_Init(void)
{
    s_start_ms = host_now_ms();
    if (s_serial_fd < 0) {
        set_error("serial port is not configured");
        return;
    }
    (void)tcflush(s_serial_fd, TCIOFLUSH);
}

uint32_t WL_Platform_GetMillis(void)
{
    return (uint32_t)(host_now_ms() - s_start_ms);
}

void WL_Platform_DelayMs(uint32_t ms)
{
    sleep_ms(ms);
}

void WL_Platform_UART_Send(const uint8_t *data, uint16_t len)
{
    uint16_t written = 0U;
    uint64_t start_ms;
    uint32_t timeout_ms = g_wl_config.tx_timeout_ms;

    if (data == NULL || len == 0U) {
        return;
    }

    if (s_serial_fd < 0) {
        set_error("serial port is not open");
        return;
    }

    if (timeout_ms < 1000U) {
        timeout_ms = 1000U;
    }

    start_ms = host_now_ms();
    while (written < len) {
        ssize_t n = write(s_serial_fd, &data[written], (size_t)(len - written));

        if (n > 0) {
            written = (uint16_t)(written + (uint16_t)n);
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if ((uint32_t)(host_now_ms() - start_ms) >= timeout_ms) {
                set_error("serial write timeout after %u ms", (unsigned)timeout_ms);
                return;
            }
            if (!wait_fd_writable(100U) && s_io_error) {
                return;
            }
            continue;
        }

        set_error("serial write failed: %s", strerror(errno));
        return;
    }

    while (tcdrain(s_serial_fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        set_error("serial drain failed: %s", strerror(errno));
        return;
    }
}

void WL_Platform_UART_SendString(const char *str)
{
    if (str == NULL) {
        return;
    }

    WL_Platform_UART_Send((const uint8_t *)str, (uint16_t)strlen(str));
}

uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len)
{
    ssize_t n;

    if (buf == NULL || max_len == 0U) {
        return 0U;
    }

    if (s_serial_fd < 0) {
        set_error("serial port is not open");
        return 0U;
    }

    n = read(s_serial_fd, buf, (size_t)max_len);
    if (n > 0) {
        return (uint16_t)n;
    }

    if (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
        set_error("serial read failed: %s", strerror(errno));
    }

    return 0U;
}

void WL_Platform_UART_FlushRx(void)
{
    if (s_serial_fd >= 0) {
        (void)tcflush(s_serial_fd, TCIFLUSH);
    }
}

bool WL_Platform_ReadAUX(void)
{
    if (s_aux_mode == WL_POSIX_AUX_ALWAYS_READY) {
        return true;
    }

#ifdef TIOCM_CTS
    if (s_serial_fd >= 0) {
        int status = 0;
        if (ioctl(s_serial_fd, TIOCMGET, &status) == 0) {
            return (status & TIOCM_CTS) != 0;
        }
        set_error("failed to read CTS for AUX: %s", strerror(errno));
    }
#endif

    return false;
}

void WL_Platform_ResetModule(void)
{
}

void WL_Platform_DebugPrint(const char *msg)
{
    if (msg != NULL) {
        (void)fputs(msg, stderr);
        (void)fflush(stderr);
    }
}

#endif
