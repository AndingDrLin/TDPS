#include "lf_app.h"
#include "lf_platform.h"
#include "wireless_hooks.h"

/*
 * 主程序入口（巡线 + 无线通信集成版）
 * 
 * 设计原则：
 * 1. 保持巡线主循环清晰稳定
 * 2. 无线通信作为独立任务在主循环中处理
 * 3. 使用 LF_Hook_* 扩展点实现无线功能集成
 * 
 * 集成功能：
 * - 标定/巡线状态无线上报
 * - 远程车辆控制（启动/停止/速度设置）
 * - 实时状态监控
 */

int main(void)
{
    LF_Platform_BoardInit();
    
    Wireless_Hooks_Init();
    LF_App_Init();
    
    LF_Platform_DebugPrint("System Initialized - Line Following + Wireless Ready\r\n");
    
    while (1) {
        LF_App_RunStep();
        
        Wireless_App_Process();
        
        Wireless_Hooks_Process();
        
        Wireless_Hooks_ProcessCommands();
        
        LF_Platform_DelayMs(1);
    }
}
