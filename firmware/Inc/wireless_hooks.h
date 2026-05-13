#ifndef WIRELESS_HOOKS_H
#define WIRELESS_HOOKS_H

#include <stdbool.h>

/*
 * 任务2无线集成入口：
 * - Init: 初始化无线模块（失败不阻断巡线主链路）。
 * - Tick: 主循环周期调用。
 */
bool Wireless_Hooks_Init(void);
void Wireless_Hooks_Tick(void);
bool Wireless_Hooks_IsReady(void);

#endif /* WIRELESS_HOOKS_H */
