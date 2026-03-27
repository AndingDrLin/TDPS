/**
 * @file    main.c
 * @brief   无线通信模块独立测试入口。
 *
 * 本文件提供两种编译目标：
 * 1. 定义 WL_USE_STM32F1_HAL_PORT → 在真实 STM32F103 硬件上运行。
 * 2. 未定义该宏 → 使用桩实现，在 PC 上编译运行以验证逻辑。
 *
 * PC 端编译命令（示例）：
 *   gcc -I../Inc -o wl_test main.c wl_app.c wl_lora.c wl_protocol.c
 *       wl_config.c wl_platform_stub.c
 */

#include "wl_app.h"
#include "wl_config.h"
#include "wl_platform.h"

#include <stdio.h>

/* ================================================================== */
/*  PC 端桩测试主函数                                                   */
/* ================================================================== */

#ifndef WL_USE_STM32F1_HAL_PORT

int main(void)
{
    printf("========================================\n");
    printf("  无线通信模块测试（桩模式）\n");
    printf("  队伍: %u - %s\n",
           g_wl_config.team_number, g_wl_config.team_name);
    printf("========================================\n\n");

    /* 1. 初始化 */
    printf("--- 步骤 1: 初始化 ---\n");
    if (!WL_App_Init()) {
        printf("初始化失败！\n");
        return 1;
    }
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 2. 开始比赛计时 */
    printf("--- 步骤 2: 开始比赛 ---\n");
    WL_App_StartRace();
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 3. 模拟经过拱门 2.1（推进时间 5 秒） */
    printf("--- 步骤 3: 经过拱门 2.1 ---\n");
    WL_Platform_DelayMs(5000);  /* 模拟 5 秒后到达 */
    WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);
    printf("已用时间: %u ms\n", (unsigned)WL_App_GetElapsedMs());
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 4. 模拟经过拱门 2.2（再推进 10 秒） */
    printf("--- 步骤 4: 经过拱门 2.2 ---\n");
    WL_Platform_DelayMs(10000); /* 模拟 10 秒后到达 */
    WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_2);
    printf("已用时间: %u ms\n", (unsigned)WL_App_GetElapsedMs());
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 5. 测试节流（立即再次发送同一检查点） */
    printf("--- 步骤 5: 节流测试（立即重复发送） ---\n");
    WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_2);
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 6. 结束比赛 */
    printf("--- 步骤 6: 结束比赛 ---\n");
    WL_App_StopRace();
    printf("状态: %s\n\n", WL_App_GetLastStatusText());

    /* 7. Tick 调用演示 */
    printf("--- 步骤 7: Tick 调用 ---\n");
    WL_App_Tick();
    printf("应用状态: %d\n\n", (int)WL_App_GetState());

    printf("========================================\n");
    printf("  测试完成\n");
    printf("========================================\n");

    return 0;
}

#else /* WL_USE_STM32F1_HAL_PORT */

/* ================================================================== */
/*  STM32 真实硬件主函数                                                */
/* ================================================================== */

/**
 * 在真实硬件上运行时，需要 STM32CubeMX 生成的 HAL 初始化代码。
 * 此处仅展示无线模块部分的调用逻辑。
 */

/* 外部声明：HAL 库初始化函数（由 CubeMX 生成） */
extern void SystemClock_Config(void);

int main(void)
{
    /* HAL 初始化 */
    HAL_Init();
    SystemClock_Config();

    /* 无线模块初始化 */
    if (!WL_App_Init()) {
        /* 初始化失败，进入错误循环 */
        while (1) {
            WL_Platform_DelayMs(1000);
        }
    }

    /* 开始比赛计时 */
    WL_App_StartRace();

    /* 主循环 */
    while (1) {
        WL_App_Tick();

        /* 实际项目中，检查点通知由巡线模块触发：
         * WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_1);
         * WL_App_NotifyCheckpoint(WL_CHECKPOINT_ARCH_2_2);
         */
    }
}

#endif /* WL_USE_STM32F1_HAL_PORT */
