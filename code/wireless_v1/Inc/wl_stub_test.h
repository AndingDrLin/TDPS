#ifndef WL_STUB_TEST_H
#define WL_STUB_TEST_H

#include <stdint.h>

/* 仅用于 PC 桩测试的辅助接口。 */
void WL_Stub_AdvanceMillis(uint32_t ms);
void WL_Stub_InjectRxData(const uint8_t *data, uint16_t len);
void WL_Stub_ClearLastTx(void);
uint16_t WL_Stub_GetLastTx(uint8_t *buf, uint16_t max_len);
uint16_t WL_Stub_GetLastTxLen(void);

#endif /* WL_STUB_TEST_H */
