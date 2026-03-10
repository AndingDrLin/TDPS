#include "lf_app.h"
#include "lf_platform.h"

/*
 * 入口文件（巡线一期）
 * 设计原则：
 * 1. 只保留一个清晰主循环，所有业务逻辑由 LF_App_RunStep 管理。
 * 2. 中断里只做采样搬运和标志置位，控制算法统一在主循环周期执行。
 * 3. 后续接入 LoRa/雷达时不改主循环结构，只扩展 hook。
 */
int main(void)
{
    LF_Platform_BoardInit();
    LF_App_Init();

    while (1) {
        LF_App_RunStep();
    }
}
