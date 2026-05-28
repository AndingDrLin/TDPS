#ifndef WL_STUB_TEST_H
#define WL_STUB_TEST_H

#include <stdbool.h>
#include <stdint.h>

/* 仅用于 PC 桩测试的辅助接口。 */
void WL_Stub_AdvanceMillis(uint32_t ms);
void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len);
void WL_Stub_AppendRxData(const uint8_t *data, uint16_t len);
void WL_Stub_ClearLastTx(void);
uint16_t WL_Stub_GetLastTx(uint8_t *buf, uint16_t max_len);
uint16_t WL_Stub_GetLastTxLen(void);
void WL_Stub_ResetTxCount(void);
uint32_t WL_Stub_GetTxCount(void);
void WL_Stub_SetAuxReady(bool ready);
bool WL_Stub_GetAuxReady(void);
void WL_Stub_SetAutoAtResponse(bool enable);

#endif /* WL_STUB_TEST_H */
