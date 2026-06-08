/**
 * @file lf_app_sensor.c
 * @brief Sensor semantics for line detection and classification.
 *
 * Wraps raw sensor data into higher-level queries: lane on/off,
 * cross detection, right-angle side detection, and line confirmation
 * with debouncing.
 */
#include "lf_app_sensor.h"

#include "lf_app_internal.h"
#include "lf_app_util.h"
#include "lf_config.h"
#include "lf_platform.h"
#include "lf_sensor.h"
#include "lf_sensor_uart.h"

/* ==================== Digital Cache ==================== */

void LF_App_RefreshDigitalCache(void)
{
    LF_App_SetDigitalCacheValid(LF_SensorUart_GetDigitalFrame(LF_App_GetDigitalCache()));
}

/* ==================== Lane Queries ==================== */

bool LF_App_LaneOn(uint8_t index)
{
    if (index >= LF_SENSOR_COUNT) {
        return false;
    }

    if (LF_App_IsDigitalCacheValid()) {
        /* Yahboom D-frame protocol: 0 = black line detected = LED on. */
        return LF_App_GetDigitalCache()[index] == 0U;
    }

    /* Fallback: use the analog normalized value against threshold. */
    return g_app.last_frame.instant_norm[index] >= g_lf_config.line_detect_min_peak;
}

bool LF_App_LaneOff(uint8_t index)
{
    return !LF_App_LaneOn(index);
}

uint8_t LF_App_CountLanesOn(void)
{
    uint8_t i;
    uint8_t count = 0U;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (LF_App_LaneOn(i)) {
            count++;
        }
    }
    return count;
}

static uint8_t count_active_filtered(uint8_t first, uint8_t last)
{
    uint8_t i;
    uint8_t count = 0U;

    if (last >= LF_SENSOR_COUNT) {
        last = (uint8_t)(LF_SENSOR_COUNT - 1U);
    }
    for (i = first; i <= last; ++i) {
        if (g_app.last_frame.filtered_u16[i] >= g_lf_config.line_detect_min_peak) {
            count++;
        }
    }
    return count;
}

/* ==================== Pattern Detection ==================== */

bool LF_App_FrameLooksLikeCross(void)
{
    if (LF_App_IsDigitalCacheValid()) {
        return LF_App_CountLanesOn() >= 7U;
    }
    return g_app.last_frame.line_detected && g_app.last_frame.active_count >= 7U;
}

int8_t LF_App_DetectRightAngleSide(void)
{
    bool left_angle;
    bool right_angle;

    /* Left right-angle: ch0-5 on, ch7 off. */
    left_angle = LF_App_LaneOff(7U) &&
                 LF_App_LaneOn(0U) && LF_App_LaneOn(1U) && LF_App_LaneOn(2U) &&
                 LF_App_LaneOn(3U) && LF_App_LaneOn(4U) && LF_App_LaneOn(5U);

    /* Right right-angle: ch2-7 on, ch0 off. */
    right_angle = LF_App_LaneOff(0U) &&
                  LF_App_LaneOn(2U) && LF_App_LaneOn(3U) && LF_App_LaneOn(4U) &&
                  LF_App_LaneOn(5U) && LF_App_LaneOn(6U) && LF_App_LaneOn(7U);

    if (left_angle && !right_angle) {
        return -1;
    }
    if (right_angle && !left_angle) {
        return +1;
    }
    return 0;
}

bool LF_App_MiddleSensorsAligned(void)
{
    return count_active_filtered(2U, 5U) >= 3U;
}

/* ==================== Line Confirmation ==================== */

bool LF_App_LineConfirmed(uint8_t *counter, uint8_t ticks)
{
    if (ticks == 0U) {
        ticks = 1U;
    }

    LF_Sensor_ReadFrame(&g_app.last_frame);
    LF_App_RefreshDigitalCache();

    if (!g_app.last_frame.line_detected ||
        g_app.last_frame.signal_sum < g_lf_config.line_detect_min_sum) {
        if (counter != 0) {
            *counter = 0U;
        }
        return false;
    }

    if (counter == 0) {
        return true;
    }

    lf_bump_u8(counter);
    return *counter >= ticks;
}
