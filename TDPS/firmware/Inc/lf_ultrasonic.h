/**
 * @file lf_ultrasonic.h
 * @brief Ultrasonic distance sensor for overhead obstacle detection.
 *
 * Uses an HC-SR04-compatible ultrasonic sensor mounted upward to detect
 * overhead obstacles (e.g., tunnels, bridges). When an obstacle is within
 * the configured threshold distance, the line-following state machine
 * pauses, sends a LoRa checkpoint message, waits, then resumes.
 */
#ifndef LF_ULTRASONIC_H
#define LF_ULTRASONIC_H

#include <stdbool.h>
#include <stdint.h>

/** Ultrasonic sensor obstacle state. */
typedef enum {
    LF_ULTRASONIC_CLEAR = 0,   /**< No overhead obstacle detected */
    LF_ULTRASONIC_BLOCK,       /**< Overhead obstacle within threshold */
} LF_UltrasonicState;

/** Ultrasonic sensor reading result. */
typedef struct {
    LF_UltrasonicState state;  /**< Current obstacle state */
    uint16_t distance_mm;      /**< Last measured distance in mm (0 = no echo) */
    bool echo_valid;           /**< Whether the last echo was received */
    uint32_t last_update_ms;   /**< Timestamp of the last successful reading */
} LF_UltrasonicReading;

/**
 * @brief Initialize the ultrasonic sensor hardware.
 */
void LF_Ultrasonic_Init(void);

/**
 * @brief Trigger an ultrasonic measurement and update the reading.
 *
 * Call this periodically (every 60-100ms). The function sends a trigger
 * pulse, waits for the echo (non-blocking with timeout), and updates
 * the internal reading.
 *
 * @param now_ms Current timestamp in milliseconds.
 */
void LF_Ultrasonic_Tick(uint32_t now_ms);

/**
 * @brief Get the latest ultrasonic reading.
 * @return Pointer to the reading struct (always valid).
 */
const LF_UltrasonicReading *LF_Ultrasonic_GetReading(void);

#endif /* LF_ULTRASONIC_H */
