/**
 * @file lf_radar.c
 * @brief Radar distance sensor driver (F4 frame + LD2410S protocols).
 */

#include "lf_radar.h"

#include <stddef.h>
#include <string.h>

#include "lf_config.h"
#include "lf_platform.h"
#include "clamp.h"

#define LF_RADAR_F4_HEADER_SIZE (4U)
#define LF_RADAR_SIMPLE_PAYLOAD_SIZE (4U)
#define LF_RADAR_LD2410S_MINIMAL_SIZE (5U)
#define LF_RADAR_LD2410S_STANDARD_PAYLOAD_SIZE (70U)
#define LF_RADAR_LD2410S_STANDARD_SIZE (80U)
#define LF_RADAR_LD2410S_STANDARD_ENERGY_OFFSET (12U)
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

/** @brief Reset the frame parser to the initial search-for-sync state. */
static void reset_parser(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_SEARCH;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
    s_radar.minimal_index = 0U;
}

/** @brief Reset debounce counters and obstacle state to CLEAR. */
static void reset_obstacle_state(void)
{
    s_radar.danger_count = 0U;
    s_radar.warn_count = 0U;
    s_radar.clear_count = 0U;
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
}

/**
 * @brief Convert centimeters to millimeters with saturation.
 * @param distance_cm Distance in centimeters.
 * @return Distance in millimeters, clamped to UINT16_MAX.
 */
static uint16_t cm_to_mm(uint16_t distance_cm)
{
    uint32_t distance_mm = (uint32_t)distance_cm * LF_RADAR_CM_TO_MM;

    if (distance_mm > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t)distance_mm;
}

/** @brief Validate the simple-payload XOR checksum.
 *  @return true if checksum matches, false otherwise.
 */
static bool simple_payload_checksum_ok(void)
{
    uint8_t checksum = (uint8_t)(s_radar.payload[0] ^ s_radar.payload[1] ^ s_radar.payload[2]);
    return checksum == s_radar.payload[3];
}

/**
 * @brief Read a little-endian uint32 from a byte pointer.
 * @param p Pointer to at least 4 bytes.
 * @return Decoded 32-bit value.
 */
static uint32_t u32_from_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/** @brief Zero all gate energy values and reset strongest gate index. */
static void clear_gate_energy(void)
{
    memset(s_radar.state.gate_energy, 0, sizeof(s_radar.state.gate_energy));
    s_radar.state.strongest_gate = 0U;
}

/** @brief Check if the current payload bytes indicate an LD2410S standard frame.
 *  @return true if payload header matches the standard-frame signature.
 */
static bool f4_payload_is_standard_candidate(void)
{
    return s_radar.payload[0] == 0x46U &&
           s_radar.payload[1] == 0x00U &&
           s_radar.payload[2] == 0x01U;
}

/**
 * @brief Update the obstacle classification with hysteresis and debounce.
 *
 * Transitions through CLEAR -> WARN -> BLOCK with debounce counting.
 * Release from BLOCK/WARN also requires consecutive clear frames.
 *
 * @param has_target  Whether the current frame reports a target.
 * @param distance_mm Distance to the target in millimeters.
 */
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

/**
 * @brief Store a validated measurement into the radar state and update obstacle classification.
 *
 * @param now_ms       Current timestamp in milliseconds.
 * @param has_target   Whether a target was detected in this frame.
 * @param distance_mm  Measured distance in millimeters.
 * @param target_state Raw target state byte from the frame payload.
 * @param frame_type   Protocol type of the parsed frame.
 */
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

/** @brief Decode and consume a simple F4-type 4-byte payload frame. */
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
    clear_gate_energy();
    consume_measurement(now_ms,
                        target_state != 0U,
                        distance_mm,
                        target_state,
                        LF_RADAR_FRAME_SIMPLE);
}

/** @brief Decode and consume an LD2410S minimal 5-byte frame. */
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
    clear_gate_energy();
    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        LF_RADAR_FRAME_LD2410S_MINIMAL);
}

/** @brief Decode and consume an LD2410S standard 80-byte frame with per-gate energy extraction. */
static void consume_ld2410s_standard_frame(uint32_t now_ms)
{
    uint8_t target_state;
    uint16_t distance_cm;
    uint32_t max_energy = 0U;
    uint8_t i;

    if (s_radar.f4_index != LF_RADAR_LD2410S_STANDARD_SIZE ||
        s_radar.f4_frame[0] != 0xF4U ||
        s_radar.f4_frame[1] != 0xF3U ||
        s_radar.f4_frame[2] != 0xF2U ||
        s_radar.f4_frame[3] != 0xF1U ||
        s_radar.f4_frame[4] != LF_RADAR_LD2410S_STANDARD_PAYLOAD_SIZE ||
        s_radar.f4_frame[5] != 0x00U ||
        s_radar.f4_frame[6] != 0x01U ||
        s_radar.f4_frame[76] != 0xF8U ||
        s_radar.f4_frame[77] != 0xF7U ||
        s_radar.f4_frame[78] != 0xF6U ||
        s_radar.f4_frame[79] != 0xF5U) {
        s_radar.state.parse_error_count += 1U;
        return;
    }

    target_state = s_radar.f4_frame[7];
    distance_cm = (uint16_t)(s_radar.f4_frame[8] |
                             ((uint16_t)s_radar.f4_frame[9] << 8));

    for (i = 0U; i < LF_RADAR_GATE_COUNT; ++i) {
        uint8_t offset = (uint8_t)(LF_RADAR_LD2410S_STANDARD_ENERGY_OFFSET + i * 4U);
        s_radar.state.gate_energy[i] = u32_from_le(&s_radar.f4_frame[offset]);
        if (s_radar.state.gate_energy[i] >= max_energy) {
            max_energy = s_radar.state.gate_energy[i];
            s_radar.state.strongest_gate = i;
        }
    }

    consume_measurement(now_ms,
                        target_state >= 2U,
                        cm_to_mm(distance_cm),
                        target_state,
                        LF_RADAR_FRAME_LD2410S_STANDARD);
}

/** @brief Transition parser to LD2410S minimal frame mode and seed the first byte. */
static void start_minimal_frame(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_LD2410S_MINIMAL;
    s_radar.minimal_frame[0] = 0x6EU;
    s_radar.minimal_index = 1U;
    s_radar.sync_index = 0U;
    s_radar.payload_index = 0U;
    s_radar.f4_index = 0U;
}

/** @brief Transition parser to F4-header frame mode and seed the first header byte. */
static void start_f4_header(void)
{
    s_radar.parse_mode = LF_RADAR_PARSE_F4_HEADER;
    s_radar.f4_frame[0] = 0xF4U;
    s_radar.f4_index = 1U;
    s_radar.sync_index = 1U;
    s_radar.payload_index = 0U;
    s_radar.minimal_index = 0U;
}

/** @brief Process one byte while in SEARCH mode, looking for frame sync signatures. */
static void parse_search_byte(uint8_t b)
{
    if (b == 0x6EU) {
        start_minimal_frame();
    } else if (b == k_f4_frame_header[0]) {
        start_f4_header();
    }
}

/** @brief Process one byte while matching the F4 header sync sequence. */
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

/** @brief Process one byte while reading the F4 body payload. */
static void parse_f4_body_byte(uint8_t b, uint32_t now_ms)
{
    if (s_radar.f4_index >= LF_RADAR_LD2410S_STANDARD_SIZE) {
        s_radar.state.parse_error_count += 1U;
        reset_parser();
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
}

/** @brief Process one byte while reading an LD2410S minimal frame body. */
static void parse_minimal_byte(uint8_t b, uint32_t now_ms)
{
    s_radar.minimal_frame[s_radar.minimal_index++] = b;
    if (s_radar.minimal_index >= LF_RADAR_LD2410S_MINIMAL_SIZE) {
        consume_ld2410s_minimal_frame(now_ms);
        reset_parser();
    }
}

/** @brief Dispatch one received byte to the appropriate sub-parser based on current mode. */
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

/**
 * @brief Initialize the radar module to a clean state.
 */
void LF_Radar_Init(void)
{
    memset(&s_radar, 0, sizeof(s_radar));
    s_radar.state.obstacle_state = LF_RADAR_OBSTACLE_CLEAR;
    s_radar.state.frame_type = LF_RADAR_FRAME_NONE;
}

/**
 * @brief Non-blocking radar tick: read pending UART bytes and parse frames.
 *
 * Resets state if radar is disabled or a frame timeout is detected.
 *
 * @param now_ms Current timestamp in milliseconds.
 */
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
        clear_gate_energy();
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
        clear_gate_energy();
    }
}

/**
 * @brief Get the most recent parsed radar state.
 * @return Pointer to the current radar state (read-only).
 */
const LF_RadarState *LF_Radar_GetState(void)
{
    return &s_radar.state;
}
