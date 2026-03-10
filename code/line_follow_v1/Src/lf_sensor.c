#include "lf_sensor.h"

#include <limits.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"

static LF_SensorCalibration s_calib;
static float s_filtered[LF_SENSOR_COUNT];
static int32_t s_last_position = 0;

static uint16_t clamp_u16(int32_t v, uint16_t lo, uint16_t hi)
{
    if (v < (int32_t)lo) {
        return lo;
    }
    if (v > (int32_t)hi) {
        return hi;
    }
    return (uint16_t)v;
}

static uint16_t normalize_with_calib(uint16_t raw, uint16_t min_v, uint16_t max_v)
{
    if (max_v <= min_v + 4U) {
        /*
         * 标定无效或动态范围过小时，回退到 12bit ADC 估算。
         * 该分支用于“首次上电还没标定完成”的安全兜底。
         */
        return clamp_u16(((int32_t)raw * 1000) / 4095, 0U, 1000U);
    }
    return clamp_u16(((int32_t)(raw - min_v) * 1000) / (int32_t)(max_v - min_v), 0U, 1000U);
}

void LF_Sensor_Init(void)
{
    uint32_t i;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_calib.min_raw[i] = UINT16_MAX;
        s_calib.max_raw[i] = 0U;
        s_filtered[i] = 0.0f;
    }
    s_calib.calibrated = false;
    s_last_position = 0;
}

void LF_Sensor_StartCalibration(void)
{
    uint32_t i;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_calib.min_raw[i] = UINT16_MAX;
        s_calib.max_raw[i] = 0U;
    }
    s_calib.calibrated = false;
}

void LF_Sensor_UpdateCalibration(void)
{
    uint16_t raw[LF_SENSOR_COUNT];
    uint32_t i;

    LF_Platform_ReadLineSensorRaw(raw);

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (raw[i] < s_calib.min_raw[i]) {
            s_calib.min_raw[i] = raw[i];
        }
        if (raw[i] > s_calib.max_raw[i]) {
            s_calib.max_raw[i] = raw[i];
        }
    }
}

void LF_Sensor_EndCalibration(void)
{
    uint32_t i;
    bool ok = true;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (s_calib.max_raw[i] <= s_calib.min_raw[i] + 20U) {
            ok = false;
            break;
        }
    }
    s_calib.calibrated = ok;
}

void LF_Sensor_ReadFrame(LF_SensorFrame *out_frame)
{
    uint32_t i;
    int64_t weighted_sum = 0;
    uint32_t signal_sum = 0U;

    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    LF_Platform_ReadLineSensorRaw(out_frame->raw);

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t norm = normalize_with_calib(out_frame->raw[i], s_calib.min_raw[i], s_calib.max_raw[i]);
        float filt = g_lf_config.sensor_filter_alpha * (float)norm +
                     (1.0f - g_lf_config.sensor_filter_alpha) * s_filtered[i];

        s_filtered[i] = filt;
        out_frame->norm[i] = norm;
        out_frame->filtered[i] = filt;
        out_frame->filtered_u16[i] = clamp_u16((int32_t)filt, 0U, 1000U);

        signal_sum += out_frame->filtered_u16[i];
        weighted_sum += (int64_t)g_lf_config.sensor_weights[i] * (int64_t)out_frame->filtered_u16[i];
    }

    out_frame->signal_sum = signal_sum;
    out_frame->line_detected = (signal_sum >= g_lf_config.line_detect_min_sum);

    if (out_frame->line_detected && signal_sum > 0U) {
        out_frame->position = (int32_t)(weighted_sum / (int64_t)signal_sum);
        s_last_position = out_frame->position;
    } else {
        /*
         * 丢线时继续输出上一次位置，便于恢复策略根据“最后方向”转向。
         */
        out_frame->position = s_last_position;
    }
}

const LF_SensorCalibration *LF_Sensor_GetCalibration(void)
{
    return &s_calib;
}
