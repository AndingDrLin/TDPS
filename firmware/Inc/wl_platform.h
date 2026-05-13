/**
 * @file    wl_platform.h
 * @brief   无线模块平台抽象层（PAL）接口定义。
 *
 * 提供与硬件无关的 UART、GPIO 和定时接口。
 * 具体实现位于 wl_platform_stm32f4.c（真实硬件）或
 * wl_platform_stub.c（离线测试桩）。
 */

#ifndef WL_PLATFORM_H
#define WL_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  板级初始化                                                         */
/* ------------------------------------------------------------------ */

/** 初始化时钟、GPIO、UART 及无线子系统所需的所有外设。
 *  启动时调用一次。 */
void WL_Platform_Init(void);

/* ------------------------------------------------------------------ */
/*  计时功能                                                           */
/* ------------------------------------------------------------------ */

/** 返回单调递增的毫秒计数器值。 */
uint32_t WL_Platform_GetMillis(void);

/** 毫秒级阻塞延时。仅建议在初始化阶段使用，控制循环中不要调用。 */
void WL_Platform_DelayMs(uint32_t ms);

/* ------------------------------------------------------------------ */
/*  UART — 与 LoRa 模块的串口通信                                      */
/* ------------------------------------------------------------------ */

/** 通过 LoRa 模块所连接的 UART 发送字节数组。
 *  @param data  数据指针
 *  @param len   要发送的字节数
 */
void WL_Platform_UART_Send(const uint8_t *data, uint16_t len);

/** 通过 LoRa 模块 UART 发送以 '\0' 结尾的字符串。 */
void WL_Platform_UART_SendString(const char *str);

/** 尝试从 UART 接收缓冲区读取最多 max_len 个字节。
 *  @param buf      目标缓冲区
 *  @param max_len  最大读取字节数
 *  @return         实际读取的字节数（无数据则返回 0）
 */
uint16_t WL_Platform_UART_Receive(uint8_t *buf, uint16_t max_len);

/** 清空 UART 接收缓冲区中所有待处理的数据。 */
void WL_Platform_UART_FlushRx(void);

/* ------------------------------------------------------------------ */
/*  GPIO — AUX 引脚                                                    */
/* ------------------------------------------------------------------ */

/** 读取 EWM22A 模块的 AUX 输出引脚状态。
 *  高电平 = 模块空闲/就绪。
 *  低电平 = 模块忙碌（正在发射、自检等）。 */
bool WL_Platform_ReadAUX(void);

/* ------------------------------------------------------------------ */
/*  调试输出                                                           */
/* ------------------------------------------------------------------ */

/** 轻量级调试打印（可路由到独立 UART 或 SWO）。
 *  允许空实现。 */
void WL_Platform_DebugPrint(const char *msg);

#endif /* WL_PLATFORM_H */
