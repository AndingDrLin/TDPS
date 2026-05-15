#ifndef LF_SENSOR_H
#define LF_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

typedef struct {
    uint16_t min_raw[LF_SENSOR_COUNT];
    uint16_t max_raw[LF_SENSOR_COUNT];
    uint16_t bad_mask;
    bool calibrated;
} LF_SensorCalibration;

typedef struct {
    uint16_t raw[LF_SENSOR_COUNT];
    uint16_t norm[LF_SENSOR_COUNT];
    float filtered[LF_SENSOR_COUNT];
    uint16_t filtered_u16[LF_SENSOR_COUNT];
    uint32_t signal_sum;
    uint16_t peak_value;
    uint8_t peak_index;
    float line_confidence;
    int8_t edge_hint; /* -1: left stronger, +1: right stronger, 0: unknown */
    int32_t position;
    bool line_detected;
} LF_SensorFrame;

/* 初始化传感器模块。 */
void LF_Sensor_Init(void);

/* 开始光照标定（重置 min/max）。 */
void LF_Sensor_StartCalibration(void);

/* 在标定阶段反复调用，用当前采样更新 min/max。 */
void LF_Sensor_UpdateCalibration(void);

/* 结束标定，生成可用标定参数。 */
void LF_Sensor_EndCalibration(void);

/* 读取一帧已处理传感器数据。 */
void LF_Sensor_ReadFrame(LF_SensorFrame *out_frame);

/* 获取当前标定结果。 */
const LF_SensorCalibration *LF_Sensor_GetCalibration(void);

#endif /* LF_SENSOR_H */
