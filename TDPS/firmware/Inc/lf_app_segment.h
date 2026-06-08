/**
 * @file lf_app_segment.h
 * @brief Segment type detection for adaptive PD control.
 *
 * Classifies the current track segment (straight, gentle curve, tight
 * curve, wide line, lost) using sensor features and temporal history.
 * Each segment type selects a different set of kp/kd/kff/speed parameters.
 */
#ifndef LF_APP_SEGMENT_H
#define LF_APP_SEGMENT_H

#include <stdint.h>

/**
 * @brief Update the direction history ring buffer.
 *
 * Tracks position sign changes to detect continuous curves via
 * frequent direction switches.
 */
void LF_App_UpdateDirectionHistory(void);

/**
 * @brief Classify the current sensor frame into a segment type candidate.
 *
 * Uses active channel count, position offset, edge hints, and
 * right-angle detection heuristics.
 *
 * @return The candidate LF_SegmentType.
 */
uint8_t LF_App_ChooseSegmentCandidate(void);

/**
 * @brief Run segment detection with confirmation and hold hysteresis.
 *
 * Updates g_app.segment_type only after the candidate has been stable
 * for segment_confirm_ticks frames, then holds for segment_hold_ticks.
 */
void LF_App_DetectSegmentType(void);

#endif /* LF_APP_SEGMENT_H */
