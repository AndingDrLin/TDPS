#include "lf_sensor.h"

#include <limits.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"
#include "clamp.h"

static LF_SensorCalibration s_calib;
static float s_filtered[LF_SENSOR_COUNT];
static int32_t s_last_position = 0;

static uint16_t normalize_with_calib(uint16_t raw, uint16_t min_v, uint16_t max_v)
{
    if (max_v <= min_v + 4U) {
        /*
         * 标定无效或动态范围过小时，回退到 12bit ADC 估算。
         * 该分支用于“首次上电还没标定完成”的安全兜底。
         */
        return TDPS_ClampU16(((int32_t)raw * 1000) / 4095, 0U, 1000U);
    }
    return TDPS_ClampU16(((int32_t)(raw - min_v) * 1000) / (int32_t)(max_v - min_v), 0U, 1000U);
}

static uint16_t normalize_digital_level(uint16_t raw)
{
    bool level_high = (raw >= g_lf_config.sensor_digital_threshold);
    bool active = g_lf_config.sensor_digital_active_high ? level_high : (!level_high);
    return active ? 1000U : 0U;
}

void LF_Sensor_Init(void)
{
    uint32_t i;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_calib.min_raw[i] = UINT16_MAX;
        s_calib.max_raw[i] = 0U;
        s_filtered[i] = 0.0f;
    }
    s_calib.bad_mask = 0U;
    s_calib.calibrated = false;
    s_last_position = 0;
}

void LF_Sensor_StartCalibration(void)
{
    uint32_t i;

    if (!g_lf_config.sensor_use_dynamic_calibration ||
        g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            s_calib.min_raw[i] = 0U;
            s_calib.max_raw[i] = 4095U;
        }
        s_calib.bad_mask = 0U;
        s_calib.calibrated = true;
        return;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_calib.min_raw[i] = UINT16_MAX;
        s_calib.max_raw[i] = 0U;
    }
    s_calib.bad_mask = 0U;
    s_calib.calibrated = false;
}

void LF_Sensor_UpdateCalibration(void)
{
    uint16_t raw[LF_SENSOR_COUNT];
    uint32_t i;

    if (!g_lf_config.sensor_use_dynamic_calibration ||
        g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        return;
    }

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

    if (!g_lf_config.sensor_use_dynamic_calibration ||
        g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        s_calib.bad_mask = 0U;
        s_calib.calibrated = true;
        return;
    }

    s_calib.bad_mask = 0U;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (s_calib.max_raw[i] <= s_calib.min_raw[i] + 20U) {
            s_calib.bad_mask |= (uint16_t)(1U << i);
            ok = false;
        }
    }
    s_calib.calibrated = ok;
}

void LF_Sensor_ReadFrame(LF_SensorFrame *out_frame)
{
    uint32_t i;
    int64_t weighted_sum = 0;
    uint32_t signal_sum = 0U;
    uint32_t left_sum = 0U;
    uint32_t right_sum = 0U;
    uint16_t peak_value = 0U;
    uint8_t peak_index = 0U;

    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    LF_Platform_ReadLineSensorRaw(out_frame->raw);

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t norm;

        if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
            norm = normalize_digital_level(out_frame->raw[i]);
        } else {
            norm = normalize_with_calib(out_frame->raw[i], s_calib.min_raw[i], s_calib.max_raw[i]);
            if (g_lf_config.sensor_invert_polarity) {
                norm = (uint16_t)(1000U - norm);
            }
        }

        float filt = g_lf_config.sensor_filter_alpha * (float)norm +
                     (1.0f - g_lf_config.sensor_filter_alpha) * s_filtered[i];

        s_filtered[i] = filt;
        out_frame->norm[i] = norm;
        out_frame->filtered[i] = filt;
        out_frame->filtered_u16[i] = TDPS_ClampU16((int32_t)filt, 0U, 1000U);

        if (out_frame->filtered_u16[i] > peak_value) {
            peak_value = out_frame->filtered_u16[i];
            peak_index = (uint8_t)i;
        }

        if (i < (LF_SENSOR_COUNT / 2U)) {
            left_sum += out_frame->filtered_u16[i];
        } else {
            right_sum += out_frame->filtered_u16[i];
        }

        signal_sum += out_frame->filtered_u16[i];
        weighted_sum += (int64_t)g_lf_config.sensor_weights[i] * (int64_t)out_frame->filtered_u16[i];
    }

    out_frame->signal_sum = signal_sum;
    out_frame->peak_value = peak_value;
    out_frame->peak_index = peak_index;
    out_frame->line_confidence = (float)peak_value / 1000.0f;

    if (left_sum > (right_sum + g_lf_config.edge_hint_threshold)) {
        out_frame->edge_hint = -1;
    } else if (right_sum > (left_sum + g_lf_config.edge_hint_threshold)) {
        out_frame->edge_hint = +1;
    } else {
        out_frame->edge_hint = 0;
    }

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
