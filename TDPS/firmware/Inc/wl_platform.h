/**
 * @file    wl_platform.h
 * @brief   Wireless module Platform Abstraction Layer (PAL) interface.
 *
 * Provides hardware-independent UART, GPIO, and timing interfaces.
 * Concrete implementations reside in wl_platform_stm32f4.c (real hardware)
 * or wl_platform_stub.c (offline test stub).
 */

#ifndef WL_PLATFORM_H
#define WL_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Board initialization                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialize clocks, GPIO, UART, and all peripherals required by
 *        the wireless subsystem. Called once at startup.
 */
void WL_Platform_Init(void);

/* ------------------------------------------------------------------ */
/*  Timing functions                                                   */
/* ------------------------------------------------------------------ */

/** @brief Return the monotonically increasing millisecond counter value. */
uint32_t WL_Platform_GetMillis(void);

/**
 * @brief Blocking millisecond delay.
 *
 * Recommended for initialization phase only; do not call in the control loop.
 *
 * @param ms Delay duration in milliseconds.
 */
void WL_Platform_DelayMs(uint32_t ms);

/* ------------------------------------------------------------------ */
/*  UART -- serial communication with the LoRa module                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Send a byte array via the UART connected to the LoRa module.
 * @param data  Pointer to the data.
 * @param len   Number of bytes to send.
 */
void WL_Platform_UART_Send(const uint8_t *data, uint16_t len);

/** @brief Send a null-terminated string via the LoRa module UART.
 *  @param str  The string to send.
 */
void WL_Platform_UART_SendString(const char *str);

/**
 * @brief Attempt to read up to max_len bytes from the UART receive buffer.
 * @param[out] buf      Destination buffer.
 * @param      max_len  Maximum number of bytes to read.
 * @return     Actual number of bytes read (0 if no data available).
 */
uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len);

/** @brief Flush all pending data from the UART receive buffer. */
void WL_Platform_UART_FlushRx(void);

/* ------------------------------------------------------------------ */
/*  GPIO -- AUX pin                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Read the AUX output pin state of the EWM22A module.
 *
 * High = module idle/ready.
 * Low  = module busy (transmitting, self-testing, etc.).
 *
 * @return true if AUX is high (ready), false if low (busy).
 */
bool WL_Platform_ReadAUX(void);

/**
 * @brief Perform a hardware reset of the EWM22A module.
 *
 * Platforms without RST wiring may provide an empty implementation.
 */
void WL_Platform_ResetModule(void);

/* ------------------------------------------------------------------ */
/*  Debug output                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Lightweight debug print (may be routed to a separate UART or SWO).
 *
 * May be an empty implementation.
 *
 * @param msg Null-terminated debug message string.
 */
void WL_Platform_DebugPrint(const char *msg);

#endif /* WL_PLATFORM_H */
