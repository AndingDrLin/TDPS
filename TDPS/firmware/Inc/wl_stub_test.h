/**
 * @file wl_stub_test.h
 * @brief Helper interfaces for PC stub testing only.
 */
#ifndef WL_STUB_TEST_H
#define WL_STUB_TEST_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Advance the stub millisecond clock.
 *  @param ms Number of milliseconds to advance.
 */
void WL_Stub_AdvanceMillis(uint32_t ms);

/**
 * @brief Inject data into the stub UART receive buffer (replaces existing content).
 * @param data  Pointer to the data.
 * @param len   Data length in bytes.
 */
void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len);

/**
 * @brief Append data to the stub UART receive buffer.
 * @param data  Pointer to the data.
 * @param len   Data length in bytes.
 */
void WL_Stub_AppendRxData(const uint8_t *data, uint16_t len);

/** @brief Clear the last transmitted data snapshot. */
void WL_Stub_ClearLastTx(void);

/**
 * @brief Retrieve the last transmitted data.
 * @param[out] buf      Destination buffer.
 * @param      max_len  Maximum bytes to copy.
 * @return     Actual number of bytes copied.
 */
uint16_t WL_Stub_GetLastTx(uint8_t *buf, uint16_t max_len);

/** @brief Get the length of the last transmitted data.
 *  @return Length in bytes.
 */
uint16_t WL_Stub_GetLastTxLen(void);

/** @brief Reset the cumulative transmission count. */
void WL_Stub_ResetTxCount(void);

/** @brief Get the cumulative transmission count.
 *  @return Total number of transmissions since last reset.
 */
uint32_t WL_Stub_GetTxCount(void);

/**
 * @brief Set the stub AUX pin ready state.
 * @param ready true = AUX high (ready), false = AUX low (busy).
 */
void WL_Stub_SetAuxReady(bool ready);

/** @brief Get the current stub AUX pin state.
 *  @return true if AUX is high (ready).
 */
bool WL_Stub_GetAuxReady(void);

/**
 * @brief Enable or disable automatic AT command responses in the stub.
 * @param enable true to enable, false to disable.
 */
void WL_Stub_SetAutoAtResponse(bool enable);

#endif /* WL_STUB_TEST_H */
