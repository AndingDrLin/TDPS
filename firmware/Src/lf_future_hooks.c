#include "lf_future_hooks.h"

#if defined(__GNUC__)
#define LF_WEAK __attribute__((weak))
#else
#define LF_WEAK
#endif

/*
 * 默认空实现（弱符号）
 * 你们后续接 LoRa/雷达时，在其他 .c 文件中提供同名非 weak 函数即可覆盖。
 */
LF_WEAK void LF_Hook_OnCalibrationBegin(void) {}
LF_WEAK void LF_Hook_OnCalibrationComplete(bool success) { (void)success; }
LF_WEAK void LF_Hook_OnLineLost(void) {}
LF_WEAK void LF_Hook_OnLineRecovered(void) {}
LF_WEAK void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id) { (void)checkpoint_id; }
LF_WEAK void LF_Hook_OnReservedObstacleWindow(void) {}
