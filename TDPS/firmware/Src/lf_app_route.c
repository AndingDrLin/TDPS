/**
 * @file lf_app_route.c
 * @brief Fixed-route script for the competition course.
 *
 * The route script defines a sequence of intersection types the car is
 * expected to encounter on the competition track. At each intersection
 * the script "peeks" at the sensor pattern and returns the expected
 * turn direction without directly modifying the active route phase.
 * The caller (lf_app_avoid or lf_app) decides whether to commit
 * the pending phase transition.
 *
 * Route sequence:
 *   WAIT_ARM -> T_RIGHT -> LEFT_1 -> CROSS(x2) -> LEFT_2 ->
 *   RIGHT_1 -> RIGHT_STRAIGHT -> RIGHT_2 -> T_LEFT -> LEFT_3 -> DONE
 */
#include "lf_app_route.h"

#include "lf_app_internal.h"
#include "lf_app_sensor.h"
#include "lf_app_util.h"
#include "lf_config.h"
#include "lf_platform.h"

/* ==================== Cooldown & Confirmation ==================== */

bool LF_App_FixedTurnCooldownActive(uint32_t now_ms)
{
    return g_lf_config.fixed_turn_cooldown_ms > 0U &&
           g_app.route_last_event_ms > 0U &&
           (uint32_t)(now_ms - g_app.route_last_event_ms) < g_lf_config.fixed_turn_cooldown_ms;
}

void LF_App_RouteSetPending(uint8_t phase)
{
    *LF_App_GetRoutePendingPhase() = phase;
}

bool LF_App_RouteCandidateConfirmed(bool candidate)
{
    uint8_t required = g_lf_config.route_event_confirm_ticks;

    if (required == 0U) {
        required = 1U;
    }
    if (!candidate) {
        g_app.route_event_confirm_count = 0U;
        return false;
    }

    lf_bump_u8(&g_app.route_event_confirm_count);
    return g_app.route_event_confirm_count >= required;
}

void LF_App_RouteClearConfirm(void)
{
    g_app.route_event_confirm_count = 0U;
}

void LF_App_RouteCommitPending(void)
{
    uint8_t *pending = LF_App_GetRoutePendingPhase();

    if (*pending != 0xFFU) {
        g_app.route_phase = *pending;
        *pending = 0xFFU;
    }
    LF_App_RouteClearConfirm();
}

/* ==================== Route Script ==================== */

bool LF_App_RouteScriptPeek(int8_t *turn_dir, uint8_t *event)
{
    bool all_on;
    int8_t right_angle_side;

    if (turn_dir != 0) {
        *turn_dir = 0;
    }
    if (event != 0) {
        *event = (uint8_t)LF_ROUTE_EVENT_NONE;
    }
    *LF_App_GetRoutePendingPhase() = 0xFFU;

    /* Route script requires both route script and fixed turn to be enabled. */
    if (!g_lf_config.route_script_enable || !g_lf_config.fixed_turn_enable) {
        LF_App_RouteClearConfirm();
        return false;
    }

    /* Respect inter-event cooldown to avoid double-triggering. */
    if (g_lf_config.route_event_cooldown_ms > 0U &&
        g_app.route_last_event_ms > 0U &&
        (uint32_t)(LF_Platform_GetMillis() - g_app.route_last_event_ms) < g_lf_config.route_event_cooldown_ms) {
        LF_App_RouteClearConfirm();
        return false;
    }

    all_on = LF_App_FrameLooksLikeCross();
    right_angle_side = LF_App_DetectRightAngleSide();

    switch ((LF_RoutePhase)g_app.route_phase) {
    case LF_ROUTE_PHASE_WAIT_ARM:
    case LF_ROUTE_PHASE_T_RIGHT:
        /* First T-intersection after S-curve: turn right.
         * Accept both all-on cross pattern AND right-angle right pattern,
         * because real T-intersections often show as 6 channels (one side off). */
        if (!all_on && right_angle_side <= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;  /* Candidate matched, suppress independent fallback. */
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_FIRST_T_RIGHT;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_LEFT_1);
        return true;

    case LF_ROUTE_PHASE_LEFT_1:
        /* Left right-angle: turn left. */
        if (right_angle_side >= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_CROSS);
        g_app.route_cross_count = 0U;
        g_app.route_cross_armed = false;
        return true;

    case LF_ROUTE_PHASE_CROSS: {
        /* Cross intersection x2: go straight, count disarmed-rearm cycles.
         * Also accept right-angle patterns: if we see a right-angle while in
         * CROSS, it means we missed the cross or it was narrow — advance anyway. */
        if (right_angle_side != 0 && g_app.route_cross_armed) {
            /* Saw a right-angle while armed: treat as a cross passage. */
            if (!LF_App_RouteCandidateConfirmed(true)) {
                return true;
            }
            LF_App_RouteClearConfirm();
            g_app.route_cross_armed = false;
            g_app.route_cross_count++;
            if (g_app.route_cross_count >= 2U) {
                LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_LEFT_2);
            }
            if (turn_dir != 0) *turn_dir = 0;
            return true;
        }
        if (!all_on) {
            g_app.route_cross_armed = true;
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!g_app.route_cross_armed) {
            return true;  /* Still on the same cross; go straight, no double-count. */
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        LF_App_RouteClearConfirm();
        g_app.route_cross_armed = false;
        g_app.route_cross_count++;
        if (g_app.route_cross_count >= 2U) {
            LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_LEFT_2);
        }
        if (turn_dir != 0) *turn_dir = 0;
        return true;
    }

    case LF_ROUTE_PHASE_LEFT_2:
        /* Second left right-angle: turn left. */
        if (right_angle_side >= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_RIGHT_1);
        return true;

    case LF_ROUTE_PHASE_RIGHT_1:
        /* Right right-angle: turn right. */
        if (right_angle_side <= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_RIGHT_RIGHT_ANGLE;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_RIGHT_STRAIGHT);
        return true;

    case LF_ROUTE_PHASE_RIGHT_STRAIGHT:
        /* Right-angle but go straight (middle of a right section). */
        if (right_angle_side <= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        LF_App_RouteClearConfirm();
        g_app.route_phase = (uint8_t)LF_ROUTE_PHASE_RIGHT_2;
        if (turn_dir != 0) *turn_dir = 0;
        return true;

    case LF_ROUTE_PHASE_RIGHT_2:
        /* Second right-angle: turn right. */
        if (right_angle_side <= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = +1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_RIGHT_RIGHT_ANGLE;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_T_LEFT);
        return true;

    case LF_ROUTE_PHASE_T_LEFT:
        /* T-intersection: turn left.
         * Accept both all-on and left right-angle patterns. */
        if (!all_on && right_angle_side >= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_T_LEFT;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_LEFT_3);
        return true;

    case LF_ROUTE_PHASE_LEFT_3:
        /* Final left right-angle: turn left. */
        if (right_angle_side >= 0) {
            LF_App_RouteClearConfirm();
            return false;
        }
        if (!LF_App_RouteCandidateConfirmed(true)) {
            return true;
        }
        if (turn_dir != 0) *turn_dir = -1;
        if (event != 0) *event = (uint8_t)LF_ROUTE_EVENT_LEFT_RIGHT_ANGLE;
        LF_App_RouteSetPending((uint8_t)LF_ROUTE_PHASE_DONE);
        return true;

    case LF_ROUTE_PHASE_DONE:
    default:
        LF_App_RouteClearConfirm();
        return false;
    }
}
