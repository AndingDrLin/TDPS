#ifndef WL_PORT_POSIX_SERIAL_H
#define WL_PORT_POSIX_SERIAL_H

#ifdef WL_USE_POSIX_SERIAL_PORT

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WL_POSIX_AUX_ALWAYS_READY = 0,
    WL_POSIX_AUX_CTS = 1,
} WL_PosixAuxMode;

int WL_PosixSerial_Configure(const char *port_path,
                             uint32_t baudrate,
                             WL_PosixAuxMode aux_mode);
void WL_PosixSerial_Close(void);
bool WL_PosixSerial_HadIoError(void);
const char *WL_PosixSerial_GetLastError(void);

#endif

#endif
