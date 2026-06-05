#include "lf_sensor.h"

#include <limits.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"
#include "clamp.h"

static LF_SensorCalibration s_calib;
static float s_filtered[LF_SENSOR_COUNT];
static int32_t s_last_position = 0;
static uint16_t s_median_hist[LF_SENSOR_COUNT][3];
static uint8_t s_median_idx = 0U;
static bool s_median_primed = false;

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (a > c) { uint16_t t = a; a = c; c = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    return b;
}

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
    s_calib.valid_count = 0U;
    s_calib.status = LF_SENSOR_CAL_FAILED;
    s_calib.calibrated = false;
    s_last_position = 0;
    s_median_primed = false;
}

void LF_Sensor_StartCalibration(void)
{
    uint32_t i;

    if (!g_lf_config.sensor_use_dynamic_calibration ||
        g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO ||
        g_lf_config.sensor_fast_calibration) {
        uint16_t max_val = (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_UART_PROTOCOL)
                           ? (255U * 16U) : 4095U;
        for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
            s_calib.min_raw[i] = 0U;
            s_calib.max_raw[i] = max_val;
        }
        s_calib.bad_mask = 0U;
        s_calib.valid_count = LF_SENSOR_COUNT;
        s_calib.status = LF_SENSOR_CAL_OK;
        s_calib.calibrated = true;
        return;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        s_calib.min_raw[i] = UINT16_MAX;
        s_calib.max_raw[i] = 0U;
    }
    s_calib.bad_mask = 0U;
    s_calib.valid_count = 0U;
    s_calib.status = LF_SENSOR_CAL_FAILED;
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
    uint8_t valid_count = 0U;
    uint16_t min_delta = g_lf_config.sensor_calibration_min_delta;

    if (!g_lf_config.sensor_use_dynamic_calibration ||
        g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
        s_calib.bad_mask = 0U;
        s_calib.valid_count = LF_SENSOR_COUNT;
        s_calib.status = LF_SENSOR_CAL_OK;
        s_calib.calibrated = true;
        return;
    }

    s_calib.bad_mask = 0U;
    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        if (s_calib.max_raw[i] <= s_calib.min_raw[i] + min_delta) {
            s_calib.bad_mask |= (uint16_t)(1U << i);
        } else {
            valid_count += 1U;
        }
    }

    s_calib.valid_count = valid_count;
    if (valid_count == LF_SENSOR_COUNT) {
        s_calib.status = LF_SENSOR_CAL_OK;
        s_calib.calibrated = true;
    } else if (g_lf_config.sensor_allow_degraded_calibration &&
               valid_count >= g_lf_config.sensor_min_valid_count) {
        s_calib.status = LF_SENSOR_CAL_DEGRADED;
        s_calib.calibrated = true;
    } else {
        s_calib.status = LF_SENSOR_CAL_FAILED;
        s_calib.calibrated = false;
    }
}

/*
 * 传感器帧读取：中值去噪 → 归一化 → 一阶 IIR → 边缘抑制 → 加权质心。
 *
 * 位约定：position 左负右正，目标值 0。
 * 权重表 sensor_weights[] 应左负右正、边缘绝对值大于中心，
 * 以提高边缘位置分辨率。
 *
 * 丢线时 position 保持上一次有效值，供恢复策略使用。
 */
void LF_Sensor_ReadFrame(LF_SensorFrame *out_frame)
{
    uint32_t i;
    int64_t weighted_sum = 0;
    uint32_t signal_sum = 0U;
    uint32_t left_sum = 0U;
    uint32_t right_sum = 0U;
    uint16_t peak_value = 0U;
    uint16_t min_value = 1000U;
    uint16_t active_threshold = g_lf_config.line_detect_min_peak;
    uint8_t active_count = 0U;
    uint8_t peak_index = 0U;
    uint16_t calc_u16[LF_SENSOR_COUNT];

    if (out_frame == NULL) {
        return;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    LF_Platform_ReadLineSensorRaw(out_frame->raw);

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t instant;
        bool sensor_valid = ((s_calib.bad_mask & (uint16_t)(1U << i)) == 0U);

        if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
            instant = normalize_digital_level(out_frame->raw[i]);
        } else {
            instant = normalize_with_calib(out_frame->raw[i], s_calib.min_raw[i], s_calib.max_raw[i]);
            if (g_lf_config.sensor_invert_polarity) {
                instant = (uint16_t)(1000U - instant);
            }
            if (!sensor_valid) {
                instant = 0U;
            }
        }
        out_frame->instant_norm[i] = instant;
    }

    /*
     * 3 样本中值滤波：消除传感器脉冲噪声（地面反光、颗粒等）。
     * 每个传感器维护 3 帧历史，取中值作为当前噪声抑制后的原始值。
     * 首帧用当前值填充全部历史，避免冷启动零值干扰。
     */
    {
        uint32_t mi;
        for (mi = 0U; mi < LF_SENSOR_COUNT; ++mi) {
            s_median_hist[mi][s_median_idx] = out_frame->raw[mi];
            if (!s_median_primed) {
                s_median_hist[mi][0] = out_frame->raw[mi];
                s_median_hist[mi][1] = out_frame->raw[mi];
                s_median_hist[mi][2] = out_frame->raw[mi];
            }
            out_frame->raw[mi] = median3(s_median_hist[mi][0],
                                         s_median_hist[mi][1],
                                         s_median_hist[mi][2]);
        }
        s_median_primed = true;
        s_median_idx = (s_median_idx + 1U) % 3U;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t norm;
        bool sensor_valid = ((s_calib.bad_mask & (uint16_t)(1U << i)) == 0U);

        if (g_lf_config.sensor_input_mode == LF_SENSOR_INPUT_DIGITAL_GPIO) {
            norm = normalize_digital_level(out_frame->raw[i]);
        } else {
            norm = normalize_with_calib(out_frame->raw[i], s_calib.min_raw[i], s_calib.max_raw[i]);
            if (g_lf_config.sensor_invert_polarity) {
                norm = (uint16_t)(1000U - norm);
            }
            if (!sensor_valid) {
                norm = 0U;
            }
        }

        /* 一阶 IIR 低通：alpha 越大越灵敏（噪声多），越小越平滑（延迟大） */
        float filt = g_lf_config.sensor_filter_alpha * (float)norm +
                     (1.0f - g_lf_config.sensor_filter_alpha) * s_filtered[i];

        s_filtered[i] = filt;
        out_frame->norm[i] = norm;
        out_frame->filtered[i] = filt;
        out_frame->filtered_u16[i] = TDPS_ClampU16((int32_t)filt, 0U, 1000U);
        calc_u16[i] = out_frame->filtered_u16[i];
    }

    /*
     * 边缘噪声抑制：若端点传感器（0 或 7）异常偏高而内侧邻居偏低，
     * 且中心区有真实信号，则将该端点清零——典型的反光/杂散光干扰模式。
     */
    if (g_lf_config.sensor_edge_noise_reject_enable && LF_SENSOR_COUNT >= 8U) {
        uint16_t neighbor_threshold = g_lf_config.sensor_edge_noise_neighbor_threshold;
        if (neighbor_threshold == 0U) {
            neighbor_threshold = g_lf_config.line_detect_min_peak;
        }
        if (calc_u16[0] >= active_threshold &&
            calc_u16[1] < neighbor_threshold && calc_u16[2] < neighbor_threshold &&
            calc_u16[3] >= active_threshold && calc_u16[4] >= active_threshold) {
            calc_u16[0] = 0U;
        }
        if (calc_u16[7] >= active_threshold &&
            calc_u16[5] < neighbor_threshold && calc_u16[6] < neighbor_threshold &&
            calc_u16[3] >= active_threshold && calc_u16[4] >= active_threshold) {
            calc_u16[7] = 0U;
        }
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        uint16_t calc_value = calc_u16[i];

        if (calc_value > peak_value) {
            peak_value = calc_value;
            peak_index = (uint8_t)i;
        }
        if (calc_value < min_value) {
            min_value = calc_value;
        }
        if (calc_value >= active_threshold) {
            active_count += 1U;
        }

        if (i < (LF_SENSOR_COUNT / 2U)) {
            left_sum += calc_value;
        } else {
            right_sum += calc_value;
        }

        signal_sum += calc_value;
        weighted_sum += (int64_t)g_lf_config.sensor_weights[i] * (int64_t)calc_value;
    }

    out_frame->signal_sum = signal_sum;
    out_frame->peak_value = peak_value;
    out_frame->background_value = min_value;
    out_frame->contrast_value = (peak_value > min_value) ? (uint16_t)(peak_value - min_value) : 0U;
    out_frame->active_count = active_count;
    out_frame->peak_index = peak_index;
    /* 线置信度：峰值越接近归一化上限 1000，质量越高 */
    out_frame->line_confidence = (float)peak_value / 1000.0f;

    /* 边缘方向提示：半区信号强度显著不对称时给出方向，用于丢线恢复 */
    if (left_sum > (right_sum + g_lf_config.edge_hint_threshold)) {
        out_frame->edge_hint = -1;
    } else if (right_sum > (left_sum + g_lf_config.edge_hint_threshold)) {
        out_frame->edge_hint = +1;
    } else {
        out_frame->edge_hint = 0;
    }

    /* 三条件判定在线：信号总量 + 峰值 + 对比度均达标 */
    out_frame->line_detected = (signal_sum >= g_lf_config.line_detect_min_sum &&
                                peak_value >= g_lf_config.line_detect_min_peak &&
                                out_frame->contrast_value >= g_lf_config.line_detect_min_contrast);

    if (out_frame->line_detected && signal_sum > 0U) {
        /* 加权质心法：position = Σ(weight_i × value_i) / Σ(value_i) */
        out_frame->position = (int32_t)(weighted_sum / (int64_t)signal_sum);
        s_last_position = out_frame->position;
    } else {
        /* 丢线时保持上次位置，恢复策略据此决定转向方向 */
        out_frame->position = s_last_position;
    }
}

const LF_SensorCalibration *LF_Sensor_GetCalibration(void)
{
    return &s_calib;
}
