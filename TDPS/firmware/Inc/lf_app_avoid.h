/**
 * @file lf_app_avoid.h
 * @brief Obstacle avoidance, fixed 90-degree turns, and reorientation.
 *
 * Contains the state processors for:
 * - Radar-based obstacle avoidance (PREP -> BYPASS -> REACQUIRE)
 * - Fixed 90-degree turns driven by the route script (STOP -> SPIN -> SETTLE)
 * - Right-angle reorientation with confirmation (STOP -> SPIN -> CONFIRM)
 */
#ifndef LF_APP_AVOID_H
#define LF_APP_AVOID_H

#include <stdbool.h>
#include <stdint.h>

/* ---- Radar snapshot & avoidance ---- */

/**
 * @brief Refresh the radar obstacle snapshot from LF_Radar.
 * @param now_ms Current timestamp in milliseconds.
 */
void LF_App_RefreshRadarSnapshot(uint32_t now_ms);

/**
 * @brief Choose obstacle avoidance turning direction.
 * @return -1 for left, +1 for right.
 */
int8_t LF_App_ChooseAvoidDir(void);

/**
 * @brief Initiate the obstacle avoidance sequence.
 * @param now_ms Current timestamp in milliseconds.
 */
void LF_App_StartAvoidance(uint32_t now_ms);

/** @brief Process the AVOID_PREP state (stop and wait). */
void LF_App_ProcessAvoidPrep(uint32_t now_ms);

/** @brief Process the AVOID_BYPASS state (3-phase arc maneuver). */
void LF_App_ProcessAvoidBypass(uint32_t now_ms);

/** @brief Process the AVOID_REACQUIRE state (sweep to re-find the line). */
void LF_App_ProcessAvoidReacquire(uint32_t now_ms);

/* ---- Fixed 90-degree turn ---- */

/** @brief Process the FIXED_TURN_STOP state. */
void LF_App_ProcessFixedTurnStop(uint32_t now_ms);

/** @brief Process the FIXED_TURN_SPIN state. */
void LF_App_ProcessFixedTurnSpin(uint32_t now_ms);

/** @brief Process the FIXED_TURN_SETTLE state. */
void LF_App_ProcessFixedTurnSettle(uint32_t now_ms);

/**
 * @brief Try to start a fixed turn from the route script or right-angle fallback.
 * @param now_ms Current timestamp in milliseconds.
 * @return true if a fixed turn was initiated.
 */
bool LF_App_TryStartFixedTurn(uint32_t now_ms);

/* ---- Reorientation ---- */

/**
 * @brief Start a reorientation spin after right-angle detection.
 * @param now_ms Current timestamp in milliseconds.
 * @param dir    Spin direction (-1 left, +1 right).
 */
void LF_App_StartReorient(uint32_t now_ms, int8_t dir);

/** @brief Process the REORIENT_STOP state (brief stop before spin). */
void LF_App_ProcessReorientStop(uint32_t now_ms);

/** @brief Process the REORIENT_SPIN state (rotate until middle aligned). */
void LF_App_ProcessReorientSpin(uint32_t now_ms);

/** @brief Process the REORIENT_CONFIRM state (verify alignment for N ticks). */
void LF_App_ProcessReorientConfirm(uint32_t now_ms);

#endif /* LF_APP_AVOID_H */
