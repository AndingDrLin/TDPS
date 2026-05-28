#ifndef LF_LED_BLINK_H
#define LF_LED_BLINK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LF_LED_OFF = 0,
    LF_LED_ON,
    LF_LED_SLOW_BLINK,      /* 1 Hz: 500ms on, 500ms off */
    LF_LED_FAST_BLINK,      /* 4 Hz: 125ms on, 125ms off */
    LF_LED_DOUBLE_FLASH,    /* 2 quick flashes + long pause */
    LF_LED_TRIPLE_FLASH,    /* 3 quick flashes + long pause */
    LF_LED_HEARTBEAT,       /* asymmetric short-long-short */
    LF_LED_RAPID_PULSE,     /* 8 Hz fast blink */
    LF_LED_POSTMORTEM,      /* blinks numeric code then stats */
} LF_LedPattern;

void LF_LedBlink_Init(void);
void LF_LedBlink_SetPattern(LF_LedPattern pattern);

/*
 * Set post-mortem blink codes. The engine will cycle:
 *   blink `reason_code` N times (250ms on/off), pause 2s
 *   blink `stats_code` N times (250ms on/off), pause 3s
 *   repeat
 * reason_code 1-255, stats_code 0-15 (0 = skip stats phase).
 */
void LF_LedBlink_SetPostmortemCode(uint8_t reason_code, uint8_t stats_code);

/* Call every main-loop iteration. Non-blocking. */
void LF_LedBlink_Tick(void);

#endif /* LF_LED_BLINK_H */
