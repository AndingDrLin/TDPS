/**
 * @file lf_sensor.h
 * @brief Line sensor acquisition, calibration, and processing.
 */
#ifndef LF_SENSOR_H
#define LF_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "lf_config.h"

/** Calibration status. */
typedef enum {
    LF_SENSOR_CAL_FAILED = 0,    /**< Calibration failed */
    LF_SENSOR_CAL_OK,            /**< Calibration succeeded */
    LF_SENSOR_CAL_DEGRADED,      /**< Degraded calibration (some channels invalid) */
} LF_SensorCalibrationStatus;

/** Calibration data for all sensor channels. */
typedef struct {
    uint16_t min_raw[LF_SENSOR_COUNT];   /**< Per-channel minimum raw value */
    uint16_t max_raw[LF_SENSOR_COUNT];   /**< Per-channel maximum raw value */
    uint16_t bad_mask;                   /**< Bitmask of channels that failed calibration */
    uint8_t valid_count;                 /**< Number of valid channels after calibration */
    LF_SensorCalibrationStatus status;   /**< Overall calibration status */
    bool calibrated;                     /**< Whether calibration has been completed */
} LF_SensorCalibration;

/** One frame of processed sensor data. */
typedef struct {
    uint16_t raw[LF_SENSOR_COUNT];           /**< Raw sensor values */
    uint16_t norm[LF_SENSOR_COUNT];          /**< Normalized sensor values (filtered) */
    uint16_t instant_norm[LF_SENSOR_COUNT];  /**< Instantaneous normalized values without median/IIR filtering; used for right-angle outer-side lamp-off detection */
    float filtered[LF_SENSOR_COUNT];         /**< Filtered sensor values (float) */
    uint16_t filtered_u16[LF_SENSOR_COUNT];  /**< Filtered sensor values (uint16) */
    uint32_t signal_sum;                     /**< Sum of all channel values */
    uint16_t peak_value;                     /**< Maximum channel value */
    uint16_t background_value;               /**< Background (minimum channel) value */
    uint16_t contrast_value;                 /**< Contrast (peak - background) */
    uint8_t active_count;                    /**< Number of active channels */
    uint8_t peak_index;                      /**< Index of the peak channel */
    float line_confidence;                   /**< Line detection confidence (0.0-1.0) */
    int8_t edge_hint;                        /**< -1: left stronger, +1: right stronger, 0: unknown */
    int32_t position;                        /**< Weighted position (negative = left, positive = right) */
    bool line_detected;                      /**< Whether a line is currently detected */
} LF_SensorFrame;

/** @brief Initialize the sensor module. */
void LF_Sensor_Init(void);

/** @brief Start light calibration (reset min/max). */
void LF_Sensor_StartCalibration(void);

/**
 * @brief Update calibration with current samples (call repeatedly during calibration).
 */
void LF_Sensor_UpdateCalibration(void);

/** @brief End calibration and generate usable calibration parameters. */
void LF_Sensor_EndCalibration(void);

/**
 * @brief Read one frame of processed sensor data.
 * @param[out] out_frame Pointer to the output frame structure.
 */
void LF_Sensor_ReadFrame(LF_SensorFrame *out_frame);

/** @brief Get the current calibration result.
 *  @return Pointer to the calibration data (read-only).
 */
const LF_SensorCalibration *LF_Sensor_GetCalibration(void);

#endif /* LF_SENSOR_H */
