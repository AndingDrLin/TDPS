/**
 * @file lf_app_route.h
 * @brief Fixed-route script for the competition course.
 *
 * The route script "peeks" at each intersection without directly modifying
 * the route phase. The caller decides whether to commit a pending phase
 * transition.
 */
#ifndef LF_APP_ROUTE_H
#define LF_APP_ROUTE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Check if the fixed-turn cooldown timer is still active.
 * @param now_ms Current timestamp in milliseconds.
 * @return true if within the cooldown window.
 */
bool LF_App_FixedTurnCooldownActive(uint32_t now_ms);

/**
 * @brief Set the pending route phase (deferred commit).
 * @param phase Target LF_RoutePhase value.
 */
void LF_App_RouteSetPending(uint8_t phase);

/**
 * @brief Accumulate route event confirmation ticks.
 *
 * Returns true once the candidate has been confirmed for the required
 * number of consecutive frames (g_lf_config.route_event_confirm_ticks).
 *
 * @param candidate true if the current frame matches the expected pattern.
 * @return true when confirmation threshold is reached.
 */
bool LF_App_RouteCandidateConfirmed(bool candidate);

/**
 * @brief Reset the route event confirmation counter.
 */
void LF_App_RouteClearConfirm(void);

/**
 * @brief Commit the pending route phase to the active route phase.
 */
void LF_App_RouteCommitPending(void);

/**
 * @brief Peek at the current intersection through the route script.
 *
 * The script inspects sensor patterns (all-on cross, right-angle) and
 * returns what action the current route phase expects. It does NOT
 * directly change the route phase; use LF_App_RouteCommitPending().
 *
 * @param[out] turn_dir -1 for left, +1 for right, 0 for straight.
 * @param[out] event    LF_RouteEvent value for debug/telemetry.
 * @return true if the intersection was recognized by the script.
 */
bool LF_App_RouteScriptPeek(int8_t *turn_dir, uint8_t *event);

#endif /* LF_APP_ROUTE_H */
