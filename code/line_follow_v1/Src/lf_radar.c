#include "lf_radar.h"

#include <stddef.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"

#define LF_RADAR_HEADER_SIZE (4U)
#define LF_RADAR_PAYLOAD_SIZE (4U)
#define LF_RADAR_READ_CHUNK_SIZE (32U)

static const uint8_t k_frame_header[LF_RADAR_HEADER_SIZE] = {0xF4U, 0xF3U, 0xF2U, 0xF1U};

typedef struct {
    LF_RadarState state;
    uint8_t sync_index;
    uint8_t payload_index;
    uint8_t payload[LF_RADAR_PAYLOAD_SIZE];
    uint8_t danger_count;
    uint8_t clear_count;
} LF_RadarRuntime;

static LF_RadarRuntime s_radar;

static uint8_t clamp_u8(uint32_t v, uint8_t lo, uint8_t hi)
{
    if (v < (uint32_t)lo) {
        return lo;
    }
    if (v > (uint32_t)hi) {
        return hi;
    }
    return (uint8_t)v;
}

static void reset_parser(void)
{
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
}

static void reset_obstacle_state(void)
{
    s_radar.danger_count = 0U;
    s_radar.clear_count = 0U;
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
}

static void update_obstacle_state(bool has_target, uint16_t distance_mm)
{
    const uint8_t debounce = clamp_u8(g_lf_config.radar_debounce_frames, 1U, 20U);

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
        s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_WARN;
    } else {
        s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    }
    s_radar.clear_count = 0U;
}

static void consume_frame(uint32_t now_ms)
{
    uint16_t distance_mm;
    bool has_target;
    uint8_t checksum;

    checksum = (uint8_t)(s_radar.payload[0] ^ s_radar.payload[1] ^ s_radar.payload[2]);
    if (checksum != s_radar.payload[3]) {
        s_radar.state.parse_error_count += 1U;
        return;
    }

    distance_mm = (uint16_t)(((uint16_t)s_radar.payload[1] << 8) | (uint16_t)s_radar.payload[0]);
    has_target = (s_radar.payload[2] != 0U);

    s_radar.state.frame_valid = true;
    s_radar.state.has_target = has_target;
    s_radar.state.distance_mm = distance_mm;
    s_radar.state.frame_count += 1U;
    s_radar.state.last_update_ms = now_ms;

    update_obstacle_state(has_target, distance_mm);
}

void LF_Radar_Init(void)
{
    memset(&s_radar, 0, sizeof(s_radar));
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
}

void LF_Radar_Tick(uint32_t now_ms)
{
    uint8_t chunk[LF_RADAR_READ_CHUNK_SIZE];
    uint16_t read_count;
    uint32_t i;

    if (!g_lf_config.radar_enable) {
        reset_obstacle_state();
        return;
    }

    read_count = LF_Platform_RadarRead(chunk, (uint16_t)sizeof(chunk));

    for (i = 0U; i < read_count; ++i) {
        uint8_t b = chunk[i];

        if (s_radar.sync_index < LF_RADAR_HEADER_SIZE) {
            if (b == k_frame_header[s_radar.sync_index]) {
                s_radar.sync_index += 1U;
            } else {
                s_radar.sync_index = (b == k_frame_header[0]) ? 1U : 0U;
                s_radar.payload_index = 0U;
            }
            continue;
        }

        s_radar.payload[s_radar.payload_index++] = b;
        if (s_radar.payload_index >= LF_RADAR_PAYLOAD_SIZE) {
            consume_frame(now_ms);
            reset_parser();
        }
    }

    if ((s_radar.state.obstacle_state != LF_RADAR_OBSTACLE_CLEAR) &&
        ((uint32_t)(now_ms - s_radar.state.last_update_ms) > g_lf_config.radar_frame_timeout_ms)) {
        reset_obstacle_state();
    }
}

const LF_RadarState *LF_Radar_GetState(void)
{
    return &s_radar.state;
}
