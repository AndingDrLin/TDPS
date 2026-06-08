/**
 * @file lf_radar.h
 * @brief Radar obstacle detection module.
 */
#ifndef LF_RADAR_H
#define LF_RADAR_H

#include <stdbool.h>
#include <stdint.h>

/** Obstacle state derived from radar distance and debounce. */
typedef enum {
    LF_RADAR_OBSTACLE_CLEAR = 0,   /**< No obstacle detected */
    LF_RADAR_OBSTACLE_WARN,        /**< Obstacle in warning zone (between trigger and release) */
    LF_RADAR_OBSTACLE_BLOCK,       /**< Obstacle too close, blocked */
} LF_RadarObstacleState;

/** Radar frame type identification. */
typedef enum {
    LF_RADAR_FRAME_NONE = 0,
    LF_RADAR_FRAME_SIMPLE,
    LF_RADAR_FRAME_LD2410S_MINIMAL,
    LF_RADAR_FRAME_LD2410S_STANDARD,
} LF_RadarFrameType;

#define LF_RADAR_GATE_COUNT (16U)

/** Radar parsed state. */
typedef struct {
    bool has_target;                     /**< Whether a target is currently detected */
    bool frame_valid;                    /**< Whether the latest frame was valid */
    uint8_t target_state;                /**< Raw target state byte from the frame */
    uint16_t distance_mm;                /**< Measured distance in millimeters */
    uint32_t frame_count;                /**< Total number of valid frames received */
    uint32_t parse_error_count;          /**< Number of parse errors */
    uint32_t last_update_ms;             /**< Timestamp of the last update (ms) */
    LF_RadarFrameType frame_type;        /**< Type of the last parsed frame */
    LF_RadarObstacleState obstacle_state; /**< Current obstacle classification */
    uint32_t gate_energy[LF_RADAR_GATE_COUNT]; /**< Per-gate energy values (LD2410S standard frame) */
    uint8_t strongest_gate;              /**< Index of the gate with the highest energy */
} LF_RadarState;

/** @brief Initialize the radar parser state. */
void LF_Radar_Init(void);

/**
 * @brief Non-blocking poll and parse of radar UART frames.
 *
 * Supported default test frame:
 *   F4 F3 F2 F1 <dist_lo> <dist_hi> <target> <checksum>
 *   checksum = dist_lo ^ dist_hi ^ target
 *
 * Supported LD2410S minimal frame:
 *   6E <target_state> <dist_cm_lo> <dist_cm_hi> 62
 *
 * @param now_ms Current timestamp in milliseconds.
 */
void LF_Radar_Tick(uint32_t now_ms);

/** @brief Get the most recent parsed state.
 *  @return Pointer to the current radar state (read-only).
 */
const LF_RadarState *LF_Radar_GetState(void);

#endif /* LF_RADAR_H */
