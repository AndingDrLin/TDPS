#ifndef LF_FUTURE_HOOKS_H
#define LF_FUTURE_HOOKS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * 后续功能预留钩子：
 * 1. 当前版本不实现 LoRa/雷达，但保留统一事件接口。
 * 2. 后续接入时，只需要在对应钩子里增加逻辑，主巡线流程不改。
 */

void LF_Hook_OnCalibrationBegin(void);
void LF_Hook_OnCalibrationComplete(bool success);
void LF_Hook_OnLineLost(void);
void LF_Hook_OnLineRecovered(void);

/* 预留：后续在路口/标记点触发 LoRa 发送。 */
void LF_Hook_OnReservedCheckpoint(uint32_t checkpoint_id);

/* 预留：后续在障碍区前触发雷达决策。 */
void LF_Hook_OnReservedObstacleWindow(void);

#endif /* LF_FUTURE_HOOKS_H */
