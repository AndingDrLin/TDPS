#include "lf_dgus_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lf_platform.h"
#include "lf_radar.h"

#define LF_DGUS_UPDATE_PERIOD_MS (100U)
#define LF_DGUS_UART_TIMEOUT_MS (50U)
#define LF_DGUS_CURVE_CHANNEL_MODE (0x01U)
#define LF_DGUS_CURVE_CLEAR_SAMPLES (64U)
#define LF_DGUS_CURVE_SAMPLE_MAX (1000U)

#if defined(LF_USE_STM32F4_HAL_PORT)
#include "lf_port_stm32f4_hal.h"
#endif

static uint32_t s_last_update_ms;

static uint16_t clamp_curve_sample(uint32_t raw_energy)
{
    if (raw_energy > LF_DGUS_CURVE_SAMPLE_MAX) {
        return LF_DGUS_CURVE_SAMPLE_MAX;
    }
    return (uint16_t)raw_energy;
}

static void send_raw(const uint8_t *data, uint16_t len)
{
#if defined(LF_USE_STM32F4_HAL_PORT)
    if (data == NULL || len == 0U) {
        return;
    }
    (void)HAL_UART_Transmit(&LF_PORT_SCREEN_UART_HANDLE, (uint8_t *)data, len, LF_DGUS_UART_TIMEOUT_MS);
#else
    (void)data;
    (void)len;
#endif
}

static void send_curve_samples(const uint16_t *samples, uint8_t sample_count)
{
    uint8_t frame[6U + LF_DGUS_CURVE_CLEAR_SAMPLES * 2U];
    uint8_t i;

    if (samples == NULL || sample_count == 0U || sample_count > LF_DGUS_CURVE_CLEAR_SAMPLES) {
        return;
    }

    frame[0] = 0x5AU;
    frame[1] = 0xA5U;
    frame[2] = (uint8_t)(3U + sample_count * 2U);
    frame[3] = 0x82U;
    frame[4] = 0x10U;
    frame[5] = 0x00U;

    for (i = 0U; i < sample_count; ++i) {
        frame[6U + i * 2U] = (uint8_t)(samples[i] >> 8);
        frame[7U + i * 2U] = (uint8_t)(samples[i] & 0xFFU);
    }

    send_raw(frame, (uint16_t)(6U + sample_count * 2U));
}

static void clear_curve(void)
{
    uint16_t zeros[LF_DGUS_CURVE_CLEAR_SAMPLES] = {0};
    send_curve_samples(zeros, LF_DGUS_CURVE_CLEAR_SAMPLES);
}

void LF_DgusScreen_Init(void)
{
    s_last_update_ms = LF_Platform_GetMillis();
    clear_curve();
}

void LF_DgusScreen_Tick(void)
{
    uint32_t now_ms = LF_Platform_GetMillis();
    const LF_RadarState *radar;
    uint16_t samples[LF_RADAR_GATE_COUNT];
    uint8_t i;

    if ((uint32_t)(now_ms - s_last_update_ms) < LF_DGUS_UPDATE_PERIOD_MS) {
        return;
    }
    s_last_update_ms = now_ms;

    radar = LF_Radar_GetState();
    if (radar != NULL && radar->frame_valid && radar->frame_type == LF_RADAR_FRAME_LD2410S_STANDARD) {
        for (i = 0U; i < LF_RADAR_GATE_COUNT; ++i) {
            samples[i] = clamp_curve_sample(radar->gate_energy[i]);
        }
    } else {
        memset(samples, 0, sizeof(samples));
    }

    send_curve_samples(samples, LF_RADAR_GATE_COUNT);
}
