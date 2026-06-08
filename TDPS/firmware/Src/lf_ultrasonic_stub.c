/**
 * @file lf_ultrasonic_stub.c
 * @brief PC stub implementation for the ultrasonic sensor.
 *
 * Provides no-op implementations for PC-side testing.
 * Always reports CLEAR (no overhead obstacle) unless overridden
 * by test code.
 */
#include "lf_ultrasonic_platform.h"

#ifndef LF_USE_STM32F4_HAL_PORT

/** Simulated echo state for testing (default: no obstacle). */
static bool s_stub_echo_high = false;
static uint32_t s_stub_micros = 0U;

void LF_Ultrasonic_Platform_Init(void)
{
    /* No hardware to initialize on PC. */
}

void LF_Ultrasonic_Platform_Trigger(void)
{
    /* No-op on PC. */
}

bool LF_Ultrasonic_Platform_ReadEcho(void)
{
    return s_stub_echo_high;
}

void LF_Ultrasonic_Platform_DelayUs(uint32_t us)
{
    s_stub_micros += us;
}

uint32_t LF_Ultrasonic_Platform_GetMicros(void)
{
    return s_stub_micros;
}

/* ===== Test helpers (not part of the public API) ===== */

void LF_Ultrasonic_Stub_SetEcho(bool high)
{
    s_stub_echo_high = high;
}

void LF_Ultrasonic_Stub_AdvanceMicros(uint32_t us)
{
    s_stub_micros += us;
}

#endif /* !LF_USE_STM32F4_HAL_PORT */
