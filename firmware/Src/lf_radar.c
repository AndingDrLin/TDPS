#include "lf_radar.h"

#include <stddef.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"
#include "clamp.h"

#define LF_RADAR_F4_HEADER_SIZE (4U)
#define LF_RADAR_SIMPLE_PAYLOAD_SIZE (4U)
#define LF_RADAR_LD2410S_MINIMAL_SIZE (5U)
#define LF_RADAR_LD2410S_STANDARD_SIZE (70U)
#define LF_RADAR_READ_CHUNK_SIZE (32U)
#define LF_RADAR_CM_TO_MM (10U)

static const uint8_t k_f4_frame_header[LF_RADAR_F4_HEADER_SIZE] = {0xF4U, 0xF3U, 0xF2U, 0xF1U};

typedef enum {
    LF_RADAR_PARSE_SEARCH = 0,
    LF_RADAR_PARSE_F4_HEADER,
    LF_RADAR_PARSE_F4_BODY,
    LF_RADAR_PARSE_LD2410S_MINIMAL,
} LF_RadarParseMode;

typedef struct {
    LF_RadarState state;
    LF_RadarParseMode parse_mode;
    uint8_t sync_index;
    uint8_t payload_index;
    uint8_t payload[LF_RADAR_SIMPLE_PAYLOAD_SIZE];
    uint8_t f4_frame[LF_RADAR_LD2410S_STANDARD_SIZE];
    uint8_t f4_index;
    uint8_t minimal_frame[LF_RADAR_LD2410S_MINIMAL_SIZE];
    uint8_t minimal_index;
    uint8_t danger_count;
    uint8_t warn_count;
    uint8_t clear_count;
} LF_RadarRuntime;

static LF_RadarRuntime s_radar;

static void reset_parser(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_SEARCH;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
    s_radar.minimal_index = 0U;
}

static void reset_obstacle_state(void)
{
    s_radar.danger_count = 0U;
    s_radar.warn_count = 0U;
    s_radar.clear_count = 0U;
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
}

static uint16_t cm_to_mm(uint16_t distance_cm)
{
    uint32_t distance_mm = (uint32_t)distance_cm * LF_RADAR_CM_TO_MM;

    if (distance_mm > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t)distance_mm;
}

static bool simple_payload_checksum_ok(void)
{
    uint8_t checksum = (uint8_t)(s_radar.payload[0] ^ s_radar.payload[1] ^ s_radar.payload[2]);
    return checksum == s_radar.payload[3];
}

static bool f4_payload_is_standard_candidate(void)
{
    return s_radar.payload[0] == 0x46U &&
           s_radar.payload[1] == 0x00U &&
           s_radar.payload[2] == 0x01U;
}

static void update_obstacle_state(bool has_target, uint16_t distance_mm)
{
    const uint8_t debounce = TDPS_ClampU8(g_lf_config.radar_debounce_frames, 1U, 20U);

    if (has_target && distance_mm <= g_lf_config.radar_trigger_distance_mm) {
        if (s_radar.danger_count < 255U) {
            s_radar.danger_count += 1U;
        }
        s_radar.clear_count = 0U;

        if (s_radar.danger_count >= debounce) {
            s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_BLOCK;
        }
        return;
    }

    s_radar.danger_count = 0U;

    if (s_radar.state.obstacle_state == LF_RADAR_OBSTACLE_BLOCK) {
        if ((!has_target) || distance_mm >= g_lf_config.radar_release_distance_mm) {
            if (s_radar.clear_count < 255U) {
                s_radar.clear_count += 1U;
            }
            if (s_radar.clear_count >= debounce) {
                s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
                s_radar.clear_count = 0U;
            }
        } else {
            s_radar.clear_count = 0U;
        }
        return;
    }

    if (has_target && distance_mm <= g_lf_config.radar_release_distance_mm) {
        if (s_radar.warn_count < 255U) {
            s_radar.warn_count += 1U;
        }
        s_radar.clear_count = 0U;
        if (s_radar.warn_count >= debounce) {
            s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_WARN;
        }
    } else {
        s_radar.warn_count = 0U;
        if (s_radar.state.obstacle_state == LF_RADAR_OBSTACLE_WARN) {
            if (s_radar.clear_count < 255U) {
                s_radar.clear_count += 1U;
            }
            if (s_radar.clear_count >= debounce) {
                s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
                s_radar.clear_count = 0U;
            }
        } else {
            s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
            s_radar.clear_count = 0U;
        }
    }
}

static void consume_measurement(uint32_t now_ms,
                                bool has_target,
                                uint16_t distance_mm,
                                uint8_t target_state,
                                LF_RadarFrameType frame_type)
{
    s_radar.state.frame_valid = true;
    s_radar.state.has_target = has_target;
    s_radar.state.target_state = target_state;
    s_radar.state.distance_mm = distance_mm;
    s_radar.state.frame_type = frame_type;
    s_radar.state.frame_count += 1U;
    s_radar.state.last_update_ms = now_ms;

    update_obstacle_state(has_target, distance_mm);
}

static void consume_simple_frame(uint32_t now_ms)
{
    uint16_t distance_mm;
    uint8_t target_state;

    if (!simple_payload_checksum_ok()) {
        s_radar.state.parse_error_count += 1U;
        return;
    }

    distance_mm = (uint16_t)(((uint16_t)s_radar.payload[1] << 8) | (uint16_t)s_radar.payload[0]);
    target_state = s_radar.payload[2];
    consume_measurement(now_ms,
                        target_state != 0U,
                        distance_mm,
                        target_state,
                        LF_RADAR_FRAME_SIMPLE);
}

static void consume_ld2410s_minimal_frame(uint32_t now_ms)
{
    uint8_t target_state;
    uint16_t distance_cm;

    if (s_radar.minimal_frame[4] != 0x62U) {
        s_radar.state.parse_error_count += 1U;
        return;
    }

    target_state = s_radar.minimal_frame[1];
    distance_cm = (uint16_t)(s_radar.minimal_frame[2] |
                             ((uint16_t)s_radar.minimal_frame[3] << 8));
    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        LF_RADAR_FRAME_LD2410S_MINIMAL);
}

static void consume_ld2410s_standard_frame(uint32_t now_ms)
{
    uint8_t target_state;
    uint16_t distance_cm;

    if (s_radar.f4_index != LF_RADAR_LD2410S_STANDARD_SIZE ||
        s_radar.f4_frame[0] != 0xF4U ||
        s_radar.f4_frame[1] != 0xF3U ||
        s_radar.f4_frame[2] != 0xF2U ||
        s_radar.f4_frame[3] != 0xF1U ||
        s_radar.f4_frame[4] != 0x46U ||
        s_radar.f4_frame[5] != 0x00U ||
        s_radar.f4_frame[6] != 0x01U ||
        s_radar.f4_frame[66] != 0xF8U ||
        s_radar.f4_frame[67] != 0xF7U ||
        s_radar.f4_frame[68] != 0xF6U ||
        s_radar.f4_frame[69] != 0xF5U) {
        s_radar.state.parse_error_count += 1U;
        return;
    }

    target_state = s_radar.f4_frame[7];
    distance_cm = (uint16_t)(s_radar.f4_frame[8] |
                             ((uint16_t)s_radar.f4_frame[9] << 8));
    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        LF_RADAR_FRAME_LD2410S_STANDARD);
}

static void start_minimal_frame(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_LD2410S_MINIMAL;
    s_radar.minimal_frame[0] = 0x6EU;
    s_radar.minimal_index = 1U;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
}

static void start_f4_header(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_F4_HEADER;
    s_radar.f4_frame[0] = 0xF4U;
    s_radar.f4_index = 1U;
    s_radar.sync_index = 1U;
    s_radar.payload_index = 0U;
    s_radar.minimal_index = 0U;
}

static void parse_search_byte(uint8_t b)
{
    if (b == 0x6EU) {
        start_minimal_frame();
    } else if (b == k_f4_frame_header[0]) {
        start_f4_header();
    }
}

static void parse_f4_header_byte(uint8_t b)
{
    if (b == k_f4_frame_header[s_radar.sync_index]) {
        s_radar.f4_frame[s_radar.f4_index++] = b;
        s_radar.sync_index += 1U;
        if (s_radar.sync_index >= LF_RADAR_F4_HEADER_SIZE) {
            s_radar.parse_mode = LF_RADAR_PARSE_F4_BODY;
            s_radar.payload_index = 0U;
        }
        return;
    }

    if (b == 0x6EU) {
        start_minimal_frame();
    } else if (b == k_f4_frame_header[0]) {
        start_f4_header();
    } else {
        reset_parser();
    }
}

static void parse_f4_body_byte(uint8_t b, uint32_t now_ms)
{
    if (s_radar.f4_index >= LF_RADAR_LD2410S_STANDARD_SIZE) {
        s_radar.state.parse_error_count += 1U;
        reset_parser();
        parse_search_byte(b);
        return;
    }

    s_radar.f4_frame[s_radar.f4_index++] = b;

    if (s_radar.payload_index < LF_RADAR_SIMPLE_PAYLOAD_SIZE) {
        s_radar.payload[s_radar.payload_index++] = b;
    }

    if (s_radar.payload_index < LF_RADAR_SIMPLE_PAYLOAD_SIZE) {
        return;
    }

    if (f4_payload_is_standard_candidate()) {
        if (s_radar.f4_index >= LF_RADAR_LD2410S_STANDARD_SIZE) {
            consume_ld2410s_standard_frame(now_ms);
            reset_parser();
        }
        return;
    }

    if (simple_payload_checksum_ok()) {
        consume_simple_frame(now_ms);
        reset_parser();
        return;
    }

    s_radar.state.parse_error_count += 1U;
    reset_parser();
    parse_search_byte(b);
}

static void parse_minimal_byte(uint8_t b, uint32_t now_ms)
{
    s_radar.minimal_frame[s_radar.minimal_index++] = b;
    if (s_radar.minimal_index >= LF_RADAR_LD2410S_MINIMAL_SIZE) {
        consume_ld2410s_minimal_frame(now_ms);
        reset_parser();
    }
}

static void parse_byte(uint8_t b, uint32_t now_ms)
{
    switch (s_radar.parse_mode) {
    case LF_RADAR_PARSE_SEARCH:
        parse_search_byte(b);
        break;

    case LF_RADAR_PARSE_F4_HEADER:
        parse_f4_header_byte(b);
        break;

    case LF_RADAR_PARSE_F4_BODY:
        parse_f4_body_byte(b, now_ms);
        break;

    case LF_RADAR_PARSE_LD2410S_MINIMAL:
        parse_minimal_byte(b, now_ms);
        break;

    default:
        reset_parser();
        break;
    }
}

void LF_Radar_Init(void)
{
    memset(&s_radar, 0, sizeof(s_radar));
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    s_radar.state.frame_type = LF_RADAR_FRAME_NONE;
}

void LF_Radar_Tick(uint32_t now_ms)
{
    uint8_t chunk[LF_RADAR_READ_CHUNK_SIZE];
    uint16_t read_count;
    uint32_t i;

    if (!g_lf_config.radar_enable) {
        reset_obstacle_state();
        reset_parser();
        s_radar.state.has_target = false;
        s_radar.state.frame_valid = false;
        s_radar.state.target_state = 0U;
        s_radar.state.distance_mm = 0U;
        s_radar.state.frame_type = LF_RADAR_FRAME_NONE;
        return;
    }

    read_count = LF_Platform_RadarRead(chunk, (uint16_t)sizeof(chunk));

    for (i = 0U; i < read_count; ++i) {
        parse_byte(chunk[i], now_ms);
    }

    if (s_radar.state.frame_valid &&
        ((uint32_t)(now_ms - s_radar.state.last_update_ms) > g_lf_config.radar_frame_timeout_ms)) {
        reset_obstacle_state();
        s_radar.state.has_target = false;
        s_radar.state.frame_valid = false;
        s_radar.state.target_state = 0U;
        s_radar.state.distance_mm = 0U;
        s_radar.state.frame_type = LF_RADAR_FRAME_NONE;
    }
}

const LF_RadarState *LF_Radar_GetState(void)
{
    return &s_radar.state;
}
