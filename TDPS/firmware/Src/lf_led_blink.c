/**
 * @file lf_led_blink.c
 * @brief Status LED blinking pattern engine.
 */

#include "lf_led_blink.h"

#include "lf_platform.h"

typedef struct {
    uint16_t duration_ms;
    bool     led_on;
} LF_LedSegment;

#define LF_LED_MAX_SEGMENTS 8

/* Pattern segment tables (repeating). */
static const LF_LedSegment s_seg_off[] = { {0, false} };
static const LF_LedSegment s_seg_on[] = { {0, true} };
static const LF_LedSegment s_seg_slow[] = { {500, true}, {500, false} };
static const LF_LedSegment s_seg_fast[] = { {125, true}, {125, false} };
static const LF_LedSegment s_seg_double[] = {
    {80, true}, {80, false}, {80, true}, {600, false}
};
static const LF_LedSegment s_seg_triple[] = {
    {80, true}, {80, false}, {80, true}, {80, false}, {80, true}, {500, false}
};
static const LF_LedSegment s_seg_heartbeat[] = {
    {60, true}, {60, false}, {200, true}, {60, false}, {60, true}, {500, false}
};
static const LF_LedSegment s_seg_rapid[] = { {62, true}, {62, false} };

static const struct {
    const LF_LedSegment *segs;
    uint8_t count;
} s_pattern_table[] = {
    { s_seg_off,       1 },
    { s_seg_on,        1 },
    { s_seg_slow,      2 },
    { s_seg_fast,      2 },
    { s_seg_double,    4 },
    { s_seg_triple,    6 },
    { s_seg_heartbeat, 6 },
    { s_seg_rapid,     2 },
};

typedef enum {
    PM_BLINK_REASON,
    PM_PAUSE_AFTER_REASON,
    PM_BLINK_STATS,
    PM_PAUSE_AFTER_STATS,
} LF_PostmortemPhase;

static struct {
    LF_LedPattern pattern;
    uint8_t  seg_idx;
    uint32_t seg_start_ms;
    /* post-mortem state */
    uint8_t  pm_reason_code;
    uint8_t  pm_stats_code;
    uint8_t  pm_blinks_remaining;
    uint8_t  pm_phase;
    uint32_t pm_phase_start_ms;
} s_led;

/** @brief Apply a boolean state to the status LED. */
static void apply_led(bool on)
{
    LF_Platform_SetStatusLed(on);
}

/** @brief Begin a segment at the given index, setting the LED to its initial on/off state. */
static void start_segment(uint8_t idx)
{
    s_led.seg_idx = idx;
    s_led.seg_start_ms = LF_Platform_GetMillis();
    if (s_pattern_table[s_led.pattern].count > 0) {
        const LF_LedSegment *seg = &s_pattern_table[s_led.pattern].segs[idx];
        apply_led(seg->led_on);
        /* duration_ms == 0 means permanent (ON or OFF patterns). */
    }
}

/**
 * @brief Initialize the LED blink engine to OFF state.
 */
void LF_LedBlink_Init(void)
{
    s_led.pattern = LF_LED_OFF;
    s_led.seg_idx = 0;
    s_led.seg_start_ms = 0;
    s_led.pm_reason_code = 0;
    s_led.pm_stats_code = 0;
    s_led.pm_blinks_remaining = 0;
    s_led.pm_phase = PM_BLINK_REASON;
    s_led.pm_phase_start_ms = 0;
    apply_led(false);
}

/**
 * @brief Request a new LED pattern, unless post-mortem mode is active.
 *
 * Post-mortem pattern takes priority and cannot be overridden by other patterns.
 *
 * @param pattern Desired LED blink pattern.
 */
void LF_LedBlink_SetPattern(LF_LedPattern pattern)
{
    if (s_led.pattern == pattern) {
        return;
    }
    if (s_led.pattern == LF_LED_POSTMORTEM) {
        /* post-mortem takes priority, do not override */
        return;
    }
    s_led.pattern = pattern;
    start_segment(0);
}

/**
 * @brief Enter post-mortem mode and set numeric blink codes.
 *
 * @param reason_code  Blink count for the reason phase (1-255).
 * @param stats_code   Blink count for the stats phase (0-15, 0 = skip).
 */
void LF_LedBlink_SetPostmortemCode(uint8_t reason_code, uint8_t stats_code)
{
    s_led.pattern = LF_LED_POSTMORTEM;
    s_led.pm_reason_code = reason_code;
    s_led.pm_stats_code = stats_code;
    s_led.pm_phase = PM_BLINK_REASON;
    s_led.pm_blinks_remaining = reason_code;
    s_led.pm_phase_start_ms = LF_Platform_GetMillis();
    apply_led(true);  /* start first blink */
}

/** @brief Advance the current normal-pattern segment on time expiry. */
static void tick_normal(void)
{
    uint8_t count = s_pattern_table[s_led.pattern].count;
    const LF_LedSegment *segs;
    uint32_t elapsed;

    if (count == 0) {
        return;
    }

    segs = s_pattern_table[s_led.pattern].segs;

    /* Permanent patterns (duration_ms == 0): no transition needed. */
    if (segs[s_led.seg_idx].duration_ms == 0) {
        return;
    }

    elapsed = LF_Platform_GetMillis() - s_led.seg_start_ms;
    if (elapsed >= segs[s_led.seg_idx].duration_ms) {
        uint8_t next = (uint8_t)((s_led.seg_idx + 1U) % count);
        start_segment(next);
    }
}

/** @brief Advance the post-mortem blink state machine through its four phases. */
static void tick_postmortem(void)
{
    uint32_t now = LF_Platform_GetMillis();
    uint32_t elapsed = now - s_led.pm_phase_start_ms;

    switch (s_led.pm_phase) {
    case PM_BLINK_REASON:
        /* Each blink: 250ms on, 250ms off = 500ms per blink count */
        if (elapsed >= 250U) {
            apply_led(false);  /* off period */
        }
        if (elapsed >= 500U) {
            s_led.pm_blinks_remaining--;
            if (s_led.pm_blinks_remaining == 0U) {
                s_led.pm_phase = PM_PAUSE_AFTER_REASON;
                s_led.pm_phase_start_ms = now;
                apply_led(false);
            } else {
                /* next blink */
                s_led.pm_phase_start_ms = now;
                apply_led(true);
            }
        }
        break;

    case PM_PAUSE_AFTER_REASON:
        if (elapsed >= 2000U) {
            if (s_led.pm_stats_code > 0U) {
                s_led.pm_phase = PM_BLINK_STATS;
                s_led.pm_blinks_remaining = s_led.pm_stats_code;
                s_led.pm_phase_start_ms = now;
                apply_led(true);
            } else {
                s_led.pm_phase = PM_PAUSE_AFTER_STATS;
                s_led.pm_phase_start_ms = now;
            }
        }
        break;

    case PM_BLINK_STATS:
        if (elapsed >= 250U) {
            apply_led(false);
        }
        if (elapsed >= 500U) {
            s_led.pm_blinks_remaining--;
            if (s_led.pm_blinks_remaining == 0U) {
                s_led.pm_phase = PM_PAUSE_AFTER_STATS;
                s_led.pm_phase_start_ms = now;
                apply_led(false);
            } else {
                s_led.pm_phase_start_ms = now;
                apply_led(true);
            }
        }
        break;

    case PM_PAUSE_AFTER_STATS:
        if (elapsed >= 3000U) {
            /* restart cycle */
            s_led.pm_phase = PM_BLINK_REASON;
            s_led.pm_blinks_remaining = s_led.pm_reason_code;
            s_led.pm_phase_start_ms = now;
            apply_led(true);
        }
        break;
    }
}

/**
 * @brief Non-blocking LED blink tick. Call every main-loop iteration.
 */
void LF_LedBlink_Tick(void)
{
    if (s_led.pattern == LF_LED_POSTMORTEM) {
        tick_postmortem();
    } else if (s_led.pattern != LF_LED_OFF && s_led.pattern != LF_LED_ON) {
        tick_normal();
    }
    /* OFF and ON: nothing to tick, LED state is permanent. */
}
