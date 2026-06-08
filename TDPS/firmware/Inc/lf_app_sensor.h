/**
 * @file lf_app_sensor.h
 * @brief Sensor semantics: lane detection, right-angle detection, line confirmation.
 */
#ifndef LF_APP_SENSOR_H
#define LF_APP_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Refresh the digital sensor cache from the latest UART frame.
 */
void LF_App_RefreshDigitalCache(void);

/**
 * @brief Check if a sensor lane detects the line (active).
 *
 * Uses the digital cache when available (D-frame: 0 = black line = LED on).
 * Falls back to instant_norm threshold comparison.
 *
 * @param index Sensor channel index (0..LF_SENSOR_COUNT-1).
 * @return true if the lane detects the line.
 */
bool LF_App_LaneOn(uint8_t index);

/**
 * @brief Check if a sensor lane does NOT detect the line.
 * @param index Sensor channel index.
 * @return true if the lane is off.
 */
bool LF_App_LaneOff(uint8_t index);

/**
 * @brief Count the number of lanes currently detecting the line.
 * @return Count of active lanes (0..LF_SENSOR_COUNT).
 */
uint8_t LF_App_CountLanesOn(void);

/**
 * @brief Check if the current frame looks like an all-on cross intersection.
 * @return true if 7+ channels are active.
 */
bool LF_App_FrameLooksLikeCross(void);

/**
 * @brief Detect which side a right-angle turn is on.
 *
 * Left angle: ch0-5 on, ch7 off.
 * Right angle: ch2-7 on, ch0 off.
 *
 * @return -1 for left, +1 for right, 0 for no right-angle detected.
 */
int8_t LF_App_DetectRightAngleSide(void);

/**
 * @brief Check if the middle sensors (ch2-5) are aligned on the line.
 * @return true if 3+ of the middle 4 channels detect the line.
 */
bool LF_App_MiddleSensorsAligned(void);

/**
 * @brief Accumulate line detection ticks and confirm when threshold met.
 *
 * Reads a fresh sensor frame, resets the counter if line is lost.
 *
 * @param counter Pointer to the confirmation counter.
 * @param ticks   Required consecutive ticks for confirmation.
 * @return true if the line has been detected for at least @p ticks frames.
 */
bool LF_App_LineConfirmed(uint8_t *counter, uint8_t ticks);

#endif /* LF_APP_SENSOR_H */
