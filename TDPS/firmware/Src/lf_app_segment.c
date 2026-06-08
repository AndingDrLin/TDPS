/**
 * @file lf_app_segment.c
 * @brief Segment type detection for adaptive PD control.
 *
 * Classifies the current track segment using sensor features (active
 * channel count, position offset, edge hints, right-angle patterns)
 * plus temporal history (direction switches for continuous curves).
 *
 * Segment transitions use a candidate-confirmation-hold scheme:
 * 1. A candidate is proposed each frame.
 * 2. If the same candidate persists for segment_confirm_ticks, it commits.
 * 3. After commit, the new segment is held for segment_hold_ticks to
 *    prevent rapid oscillation.
 */
#include "lf_app_segment.h"

#include "lf_app_internal.h"
#include "lf_app_sensor.h"
#include "lf_app_util.h"
#include "lf_config.h"

/* ==================== Direction History ==================== */

void LF_App_UpdateDirectionHistory(void)
{
    int8_t sign = 0;
    int8_t prev;

    /* Determine the sign of the current position. */
    if (g_app.last_frame.position < 0) {
        sign = -1;
    } else if (g_app.last_frame.position > 0) {
        sign = +1;
    }

    if (g_app.sign_history_index >= 20U) {
        g_app.sign_history_index = 0U;
    }

    prev = g_app.position_sign_history[g_app.sign_history_index];
    g_app.position_sign_history[g_app.sign_history_index] = sign;
    g_app.sign_history_index++;

    /* When the ring buffer wraps, decay the switch count by one. */
    if (g_app.sign_history_index >= 20U) {
        g_app.sign_history_index = 0U;
        if (g_app.direction_switch_count > 0U) {
            g_app.direction_switch_count--;
        }
    }

    /* Detect sign changes (direction switches) for curve detection. */
    if (sign != 0 && prev != 0 && sign != prev) {
        lf_bump_u8(&g_app.direction_switch_count);
    }
}

/* ==================== Segment Classification ==================== */

uint8_t LF_App_ChooseSegmentCandidate(void)
{
    int32_t abs_pos;

    if (!g_app.last_frame.line_detected) {
        return (uint8_t)LF_SEGMENT_LOST;
    }

    abs_pos = lf_abs_i32(g_app.last_frame.position);

    /* Tight curve: right-angle pattern, large offset, or 5+ active channels. */
    if (LF_App_DetectRightAngleSide() != 0 || abs_pos >= 850 || g_app.last_frame.active_count >= 5U) {
        return (uint8_t)LF_SEGMENT_TIGHT_CURVE;
    }

    /* Wide line / cross: 7+ channels active. */
    if (LF_App_FrameLooksLikeCross() || g_app.last_frame.active_count >= 6U) {
        return (uint8_t)LF_SEGMENT_WIDE_LINE;
    }

    /* Gentle curve: moderate offset or edge hint present. */
    if (abs_pos >= 300 || g_app.last_frame.edge_hint != 0) {
        return (uint8_t)LF_SEGMENT_GENTLE_CURVE;
    }

    return (uint8_t)LF_SEGMENT_STRAIGHT;
}

void LF_App_DetectSegmentType(void)
{
    uint8_t candidate;

    if (!g_lf_config.segment_control_enable) {
        g_app.segment_type = LF_SEGMENT_STRAIGHT;
        return;
    }

    candidate = LF_App_ChooseSegmentCandidate();

    /* Lost line: immediately commit, no confirmation needed. */
    if (candidate == (uint8_t)LF_SEGMENT_LOST) {
        g_app.segment_type = (LF_SegmentType)candidate;
        g_app.segment_candidate = (LF_SegmentType)candidate;
        g_app.segment_candidate_count = 0U;
        return;
    }

    /* Hold period: freeze the current segment type. */
    if (g_app.segment_hold_count > 0U) {
        g_app.segment_hold_count--;
        return;
    }

    /* Same as current: reset candidate counter. */
    if (candidate == (uint8_t)g_app.segment_type) {
        g_app.segment_candidate_count = 0U;
        return;
    }

    /* Track consecutive frames with the same candidate. */
    if (candidate == (uint8_t)g_app.segment_candidate) {
        lf_bump_u8(&g_app.segment_candidate_count);
    } else {
        g_app.segment_candidate = (LF_SegmentType)candidate;
        g_app.segment_candidate_count = 1U;
    }

    /* Commit when the candidate has been stable long enough. */
    if (g_app.segment_candidate_count >= g_lf_config.segment_confirm_ticks) {
        g_app.segment_type = (LF_SegmentType)candidate;
        g_app.segment_candidate_count = 0U;
        g_app.segment_hold_count = g_lf_config.segment_hold_ticks;
    }
}
