#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lf_app.h"
#include "lf_config.h"
#include "lf_platform.h"

_Static_assert(LF_SENSOR_COUNT == 4U, "lf_autotest_harness assumes 4 line sensors.");

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    TRACK_CIRCLE = 0,
    TRACK_FIGURE8,
    TRACK_PATIO_PROXY,
} TrackType;

typedef struct {
    double x;
    double y;
    double theta;
} Pose;

typedef struct {
    const char *id;
    const char *name;
    TrackType track;
    Pose start_pose;
    double noise_std;
    double dropout_prob;
    double contrast;
    double left_motor_scale;
    double right_motor_scale;
    uint32_t seed;
} Scenario;

typedef struct {
    bool line_detected;
    double line_error_m;
    double confidence;
} LineState;

typedef struct {
    const char *id;
    const char *name;
    const char *preset;
    double duration_target_sec;
    double duration_simulated_sec;
    uint32_t steps;
    double line_detection_rate;
    uint32_t line_lost_transitions;
    uint32_t line_recovered_transitions;
    double longest_lost_sec;
    double total_lost_sec;
    double mean_abs_error_m;
    double rms_error_m;
    double max_abs_error_m;
    double motor_saturation_rate;
    double distance_m;
    double score;
    bool has_runtime_error;
    char runtime_error[160];
} ScenarioResult;

typedef struct {
    double duration_sec;
    double dt_sec;
    double line_threshold;
    double line_width_m;
    double max_wheel_speed_mps;
    double track_width_m;
    const char *report_path;
    uint32_t base_seed;
} TestConfig;

typedef struct {
    uint32_t scenario_count;
    uint32_t completed_count;
    bool aborted;
    double overall_score;
    double avg_line_detection_rate;
    double max_longest_lost_sec;
} SuiteSummary;

typedef struct {
    double x;
    double y;
} Point2;

static Pose g_pose;
static int16_t g_left_cmd = 0;
static int16_t g_right_cmd = 0;
static double g_sim_time_sec = 0.0;
static bool g_led_on = false;

static TrackType g_track = TRACK_CIRCLE;
static double g_line_width_m = 0.03;
static double g_max_wheel_speed_mps = 1.0;
static double g_track_width_m = 0.2;

static const Scenario *g_active_scenario = NULL;
static uint32_t g_rng_state = 1U;
static double g_sensor_gain[LF_SENSOR_COUNT];
static double g_sensor_bias[LF_SENSOR_COUNT];

static const double k_sensor_xy[LF_SENSOR_COUNT][2] = {
    {0.16, 0.075},
    {0.16, 0.025},
    {0.16, -0.025},
    {0.16, -0.075},
};

static const Point2 k_patio_path[] = {
    {0.00, 0.00},
    {0.00, 4.00},
    {0.10, 4.18},
    {0.25, 4.27},
    {0.40, 4.18},
    {0.50, 4.00},
    {0.50, 2.25},
    {0.66, 1.92},
    {0.36, 1.56},
    {0.68, 1.20},
    {0.36, 0.86},
    {0.52, 0.52},
    {0.52, 0.18},
    {1.24, 0.18},
    {1.24, 2.45},
    {2.58, 2.45},
    {2.58, 1.50},
    {2.78, 1.24},
    {3.02, 1.12},
    {3.24, 1.30},
    {3.36, 1.58},
    {3.36, 2.02},
    {3.95, 2.02},
};

static double clamp_d(double v, double lo, double hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static double normalize_angle(double angle)
{
    while (angle > M_PI) {
        angle -= 2.0 * M_PI;
    }
    while (angle < -M_PI) {
        angle += 2.0 * M_PI;
    }
    return angle;
}

static uint32_t xorshift32(void)
{
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = (x == 0U) ? 1U : x;
    return g_rng_state;
}

static double rng_uniform01(void)
{
    return ((double)(xorshift32() >> 8)) * (1.0 / 16777216.0);
}

static double rng_symm(void)
{
    return 2.0 * rng_uniform01() - 1.0;
}

static const char *track_name(TrackType track)
{
    if (track == TRACK_CIRCLE) {
        return "circle";
    }
    if (track == TRACK_FIGURE8) {
        return "figure8";
    }
    return "patio_proxy";
}

static double dist_point_segment(double px, double py, double x1, double y1, double x2, double y2)
{
    const double vx = x2 - x1;
    const double vy = y2 - y1;
    const double wx = px - x1;
    const double wy = py - y1;
    const double vv = vx * vx + vy * vy;

    if (vv <= 1e-12) {
        return hypot(px - x1, py - y1);
    }

    {
        const double t = clamp_d((wx * vx + wy * vy) / vv, 0.0, 1.0);
        const double cx = x1 + t * vx;
        const double cy = y1 + t * vy;
        return hypot(px - cx, py - cy);
    }
}

static double distance_to_patio_centerline(double x, double y)
{
    size_t i;
    double best = 1e9;

    for (i = 0U; i + 1U < (sizeof(k_patio_path) / sizeof(k_patio_path[0])); ++i) {
        const Point2 *a = &k_patio_path[i];
        const Point2 *b = &k_patio_path[i + 1U];
        const double d = dist_point_segment(x, y, a->x, a->y, b->x, b->y);
        if (d < best) {
            best = d;
        }
    }

    return best;
}

static double distance_to_track_centerline(double x, double y)
{
    if (g_track == TRACK_CIRCLE) {
        const double r = 1.15;
        const double dx = x - r;
        const double dy = y;
        return fabs(hypot(dx, dy) - r);
    }

    if (g_track == TRACK_FIGURE8) {
        const double r = 0.7;
        const double d1 = fabs(hypot(x + r, y) - r);
        const double d2 = fabs(hypot(x - r, y) - r);
        return (d1 < d2) ? d1 : d2;
    }

    return distance_to_patio_centerline(x, y);
}

static double line_intensity(double distance_m)
{
    const double half = g_line_width_m * 0.5;

    if (half <= 1e-9) {
        return 0.0;
    }

    if (distance_m <= half) {
        return 1.0;
    }

    {
        const double sigma = half * 0.85;
        const double err = distance_m - half;
        return exp(-(err * err) / (2.0 * sigma * sigma));
    }
}

static void read_sensor_norm_and_raw(double out_norm[LF_SENSOR_COUNT], uint16_t out_raw[LF_SENSOR_COUNT])
{
    uint32_t i;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        const double local_x = k_sensor_xy[i][0];
        const double local_y = k_sensor_xy[i][1];
        const double c = cos(g_pose.theta);
        const double s = sin(g_pose.theta);
        const double wx = g_pose.x + local_x * c - local_y * s;
        const double wy = g_pose.y + local_x * s + local_y * c;
        const double dist = distance_to_track_centerline(wx, wy);
        double intensity = line_intensity(dist);
        double raw_f;
        int32_t raw_i;

        intensity *= (g_active_scenario != NULL) ? g_active_scenario->contrast : 1.0;
        intensity *= g_sensor_gain[i];
        intensity += g_sensor_bias[i];

        if (g_active_scenario != NULL && g_active_scenario->noise_std > 0.0) {
            intensity += g_active_scenario->noise_std * rng_symm();
        }

        if (g_active_scenario != NULL && g_active_scenario->dropout_prob > 0.0) {
            if (rng_uniform01() < g_active_scenario->dropout_prob) {
                intensity *= 0.20;
            }
        }

        intensity = clamp_d(intensity, 0.0, 1.0);
        raw_f = 150.0 + intensity * 3650.0;
        raw_i = clamp_i32((int32_t)llround(raw_f), 0, 4095);

        out_norm[i] = intensity;
        out_raw[i] = (uint16_t)raw_i;
    }
}

static LineState estimate_line_state(const double sensor_norm[LF_SENSOR_COUNT], double threshold)
{
    uint32_t i;
    double signal_sum = 0.0;
    double weighted_y = 0.0;
    double max_value = 0.0;
    LineState st;

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        const double v = clamp_d(sensor_norm[i], 0.0, 1.0);
        signal_sum += v;
        weighted_y += k_sensor_xy[i][1] * v;
        if (v > max_value) {
            max_value = v;
        }
    }

    st.line_detected = (max_value >= threshold);
    st.line_error_m = (signal_sum > 1e-12) ? (weighted_y / signal_sum) : 0.0;
    st.confidence = max_value;
    return st;
}

static double command_to_speed_mps(int16_t cmd)
{
    const double normalized = clamp_d((double)cmd, -1000.0, 1000.0) / 1000.0;
    return normalized * g_max_wheel_speed_mps;
}

static double update_pose_from_command(double dt_s)
{
    double v_l = command_to_speed_mps(g_left_cmd);
    double v_r = command_to_speed_mps(g_right_cmd);
    const double old_x = g_pose.x;
    const double old_y = g_pose.y;

    if (g_active_scenario != NULL) {
        v_l *= g_active_scenario->left_motor_scale;
        v_r *= g_active_scenario->right_motor_scale;
    }

    {
        const double slip_indicator = clamp_d(fabs(v_r - v_l) / (2.0 * g_max_wheel_speed_mps + 1e-9), 0.0, 1.0);
        const double traction = 1.0 - 0.20 * slip_indicator;
        v_l *= traction;
        v_r *= traction;
    }

    {
        const double vx = 0.5 * (v_l + v_r);
        const double omega = (v_r - v_l) / g_track_width_m;

        if (fabs(omega) > 1e-9) {
            const double new_theta = g_pose.theta + omega * dt_s;
            g_pose.x += (vx / omega) * (sin(new_theta) - sin(g_pose.theta));
            g_pose.y += -(vx / omega) * (cos(new_theta) - cos(g_pose.theta));
            g_pose.theta = normalize_angle(new_theta);
        } else {
            g_pose.x += vx * cos(g_pose.theta) * dt_s;
            g_pose.y += vx * sin(g_pose.theta) * dt_s;
        }
    }

    return hypot(g_pose.x - old_x, g_pose.y - old_y);
}

static double score_scenario(const ScenarioResult *r)
{
    double score;

    if (r->has_runtime_error) {
        return 0.0;
    }

    score = 100.0;
    score -= (1.0 - r->line_detection_rate) * 60.0;
    score -= fmin(r->mean_abs_error_m / 0.08, 1.0) * 20.0;
    score -= fmin(r->longest_lost_sec / 1.2, 1.0) * 15.0;
    score -= fmin(r->motor_saturation_rate / 0.6, 1.0) * 5.0;
    return clamp_d(score, 0.0, 100.0);
}

static void reset_simulation(const Scenario *scenario, const TestConfig *cfg)
{
    uint32_t i;

    g_pose = scenario->start_pose;
    g_left_cmd = 0;
    g_right_cmd = 0;
    g_sim_time_sec = 0.0;
    g_led_on = false;
    g_track = scenario->track;
    g_line_width_m = cfg->line_width_m;
    g_max_wheel_speed_mps = cfg->max_wheel_speed_mps;
    g_track_width_m = cfg->track_width_m;
    g_active_scenario = scenario;

    g_rng_state = (cfg->base_seed ^ scenario->seed);
    if (g_rng_state == 0U) {
        g_rng_state = 1U;
    }

    for (i = 0U; i < LF_SENSOR_COUNT; ++i) {
        g_sensor_gain[i] = 1.0 + 0.05 * rng_symm();
        g_sensor_bias[i] = 0.01 * rng_symm();
    }
}

static ScenarioResult run_scenario(const Scenario *scenario, const TestConfig *cfg)
{
    const uint32_t target_steps = (uint32_t)fmax(1.0, llround(cfg->duration_sec / cfg->dt_sec));
    ScenarioResult result;
    double current_lost_sec = 0.0;
    double abs_error_accum = 0.0;
    double sq_error_accum = 0.0;
    uint32_t error_samples = 0U;
    uint32_t motor_saturation_steps = 0U;
    bool prev_valid = false;
    bool prev_line_detected = false;
    uint32_t step;

    memset(&result, 0, sizeof(result));
    result.id = scenario->id;
    result.name = scenario->name;
    result.preset = track_name(scenario->track);
    result.duration_target_sec = cfg->duration_sec;

    reset_simulation(scenario, cfg);
    LF_Platform_BoardInit();
    LF_App_Init();

    for (step = 0U; step < target_steps; ++step) {
        double sensor_norm[LF_SENSOR_COUNT];
        uint16_t sensor_raw[LF_SENSOR_COUNT];
        LineState st;
        const LF_AppContext *ctx;

        read_sensor_norm_and_raw(sensor_norm, sensor_raw);
        st = estimate_line_state(sensor_norm, cfg->line_threshold);

        g_sim_time_sec += cfg->dt_sec;
        LF_App_RunStep();
        result.distance_m += update_pose_from_command(cfg->dt_sec);
        result.steps += 1U;

        if (st.line_detected) {
            result.line_detection_rate += 1.0;
            abs_error_accum += fabs(st.line_error_m);
            sq_error_accum += st.line_error_m * st.line_error_m;
            if (fabs(st.line_error_m) > result.max_abs_error_m) {
                result.max_abs_error_m = fabs(st.line_error_m);
            }
            error_samples += 1U;
            if (current_lost_sec > 0.0) {
                if (current_lost_sec > result.longest_lost_sec) {
                    result.longest_lost_sec = current_lost_sec;
                }
                current_lost_sec = 0.0;
            }
        } else {
            current_lost_sec += cfg->dt_sec;
            result.total_lost_sec += cfg->dt_sec;
            if (prev_valid && prev_line_detected) {
                result.line_lost_transitions += 1U;
            }
        }

        if (prev_valid && (!prev_line_detected) && st.line_detected) {
            result.line_recovered_transitions += 1U;
        }
        prev_line_detected = st.line_detected;
        prev_valid = true;

        if ((abs(g_left_cmd) >= (g_lf_config.max_motor_cmd - 1)) ||
            (abs(g_right_cmd) >= (g_lf_config.max_motor_cmd - 1))) {
            motor_saturation_steps += 1U;
        }

        ctx = LF_App_GetContext();
        if (ctx->state == LF_APP_STATE_FAULT) {
            result.has_runtime_error = true;
            snprintf(result.runtime_error, sizeof(result.runtime_error), "State entered FAULT");
            break;
        }
    }

    if (current_lost_sec > result.longest_lost_sec) {
        result.longest_lost_sec = current_lost_sec;
    }

    result.duration_simulated_sec = result.steps * cfg->dt_sec;
    if (result.steps > 0U) {
        result.line_detection_rate /= (double)result.steps;
        result.motor_saturation_rate = (double)motor_saturation_steps / (double)result.steps;
    }
    if (error_samples > 0U) {
        result.mean_abs_error_m = abs_error_accum / (double)error_samples;
        result.rms_error_m = sqrt(sq_error_accum / (double)error_samples);
    }

    result.score = score_scenario(&result);
    return result;
}

static bool parse_double_arg(const char *text, double min_v, double max_v, double *out)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    if (!(value >= min_v && value <= max_v)) {
        return false;
    }

    *out = value;
    return true;
}

static bool parse_u32_arg(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    if (value > 0xffffffffUL) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool write_report_json(const char *path,
                              const TestConfig *cfg,
                              const ScenarioResult *results,
                              size_t result_count,
                              const SuiteSummary *summary,
                              const char *const *issues,
                              size_t issue_count)
{
    FILE *fp;
    size_t i;
    const long now_epoch = (long)time(NULL);

    fp = fopen(path, "w");
    if (fp == NULL) {
        return false;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": 2,\n");
    fprintf(fp, "  \"generatedAtEpochSec\": %ld,\n", now_epoch);
    fprintf(fp, "  \"config\": {\n");
    fprintf(fp, "    \"durationSec\": %.6f,\n", cfg->duration_sec);
    fprintf(fp, "    \"dt\": %.6f,\n", cfg->dt_sec);
    fprintf(fp, "    \"lineThreshold\": %.6f,\n", cfg->line_threshold);
    fprintf(fp, "    \"baseSeed\": %u\n", cfg->base_seed);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"summary\": {\n");
    fprintf(fp, "    \"scenarioCount\": %u,\n", summary->scenario_count);
    fprintf(fp, "    \"completedCount\": %u,\n", summary->completed_count);
    fprintf(fp, "    \"aborted\": %s,\n", summary->aborted ? "true" : "false");
    fprintf(fp, "    \"overallScore\": %.6f,\n", summary->overall_score);
    fprintf(fp, "    \"avgLineDetectionRate\": %.6f,\n", summary->avg_line_detection_rate);
    fprintf(fp, "    \"maxLongestLostSec\": %.6f\n", summary->max_longest_lost_sec);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"issues\": [");
    for (i = 0U; i < issue_count; ++i) {
        fprintf(fp, "%s\"%s\"", (i == 0U) ? "" : ", ", issues[i]);
    }
    fprintf(fp, "],\n");

    fprintf(fp, "  \"scenarios\": [\n");
    for (i = 0U; i < result_count; ++i) {
        const ScenarioResult *r = &results[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": \"%s\",\n", r->id);
        fprintf(fp, "      \"name\": \"%s\",\n", r->name);
        fprintf(fp, "      \"preset\": \"%s\",\n", r->preset);
        fprintf(fp, "      \"durationTargetSec\": %.6f,\n", r->duration_target_sec);
        fprintf(fp, "      \"durationSimulatedSec\": %.6f,\n", r->duration_simulated_sec);
        fprintf(fp, "      \"steps\": %u,\n", r->steps);
        fprintf(fp, "      \"lineDetectionRate\": %.6f,\n", r->line_detection_rate);
        fprintf(fp, "      \"lineLostTransitions\": %u,\n", r->line_lost_transitions);
        fprintf(fp, "      \"lineRecoveredTransitions\": %u,\n", r->line_recovered_transitions);
        fprintf(fp, "      \"longestLostSec\": %.6f,\n", r->longest_lost_sec);
        fprintf(fp, "      \"totalLostSec\": %.6f,\n", r->total_lost_sec);
        fprintf(fp, "      \"meanAbsErrorM\": %.6f,\n", r->mean_abs_error_m);
        fprintf(fp, "      \"rmsErrorM\": %.6f,\n", r->rms_error_m);
        fprintf(fp, "      \"maxAbsErrorM\": %.6f,\n", r->max_abs_error_m);
        fprintf(fp, "      \"motorSaturationRate\": %.6f,\n", r->motor_saturation_rate);
        fprintf(fp, "      \"distanceM\": %.6f,\n", r->distance_m);
        fprintf(fp, "      \"runtimeError\": %s,\n",
                r->has_runtime_error ? "\"State entered FAULT\"" : "null");
        fprintf(fp, "      \"score\": %.6f\n", r->score);
        fprintf(fp, "    }%s\n", (i + 1U < result_count) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

int main(int argc, char **argv)
{
    TestConfig cfg = {
        .duration_sec = 15.0,
        .dt_sec = 0.01,
        .line_threshold = 0.12,
        .line_width_m = 0.03,
        .max_wheel_speed_mps = 1.0,
        .track_width_m = 0.2,
        .report_path = "code/line_follow_v1/tests/reports/single_run/last_autotest_report.json",
        .base_seed = 20260319U,
    };

    const Pose base_pose = {0.0, 0.0, M_PI / 2.0};
    const double left_offset = 0.04;
    const Pose offset_left = {
        .x = base_pose.x - sin(base_pose.theta) * left_offset,
        .y = base_pose.y + cos(base_pose.theta) * left_offset,
        .theta = base_pose.theta,
    };
    const Pose offset_right = {
        .x = base_pose.x + sin(base_pose.theta) * left_offset,
        .y = base_pose.y - cos(base_pose.theta) * left_offset,
        .theta = base_pose.theta,
    };

    const Scenario scenarios[] = {
        {"circle_baseline", "Circle baseline", TRACK_CIRCLE, base_pose, 0.005, 0.0, 1.0, 1.00, 1.00, 101U},
        {"figure8_baseline", "Figure8 baseline", TRACK_FIGURE8, base_pose, 0.008, 0.0, 1.0, 1.00, 1.00, 102U},
        {"patio_baseline", "Patio proxy baseline", TRACK_PATIO_PROXY, base_pose, 0.010, 0.002, 1.0, 1.00, 1.00, 201U},
        {"patio_left_offset", "Patio left offset", TRACK_PATIO_PROXY, offset_left, 0.012, 0.003, 1.0, 1.00, 1.00, 202U},
        {"patio_right_offset", "Patio right offset", TRACK_PATIO_PROXY, offset_right, 0.012, 0.003, 1.0, 1.00, 1.00, 203U},
        {"patio_noisy", "Patio noisy sensors", TRACK_PATIO_PROXY, base_pose, 0.028, 0.010, 0.94, 1.00, 1.00, 204U},
        {"patio_motor_bias", "Patio motor mismatch", TRACK_PATIO_PROXY, base_pose, 0.015, 0.004, 0.97, 0.93, 1.07, 205U},
        {"patio_stress", "Patio combined stress", TRACK_PATIO_PROXY, offset_left, 0.032, 0.015, 0.90, 0.92, 1.08, 206U},
    };

    ScenarioResult results[sizeof(scenarios) / sizeof(scenarios[0])];
    const size_t scenario_count = sizeof(scenarios) / sizeof(scenarios[0]);
    SuiteSummary summary;
    const char *issues[16];
    size_t issue_count = 0U;
    size_t i;
    uint32_t completed = 0U;
    double score_sum = 0.0;
    double detect_sum = 0.0;
    double max_longest = 0.0;
    char runtime_ids[256];
    size_t runtime_issue_count = 0U;
    uint32_t low_score_count = 0U;

    runtime_ids[0] = '\0';

    if (argc >= 2 && !parse_double_arg(argv[1], 1.0, 180.0, &cfg.duration_sec)) {
        fprintf(stderr, "Invalid durationSec: %s (expected 1..180)\n", argv[1]);
        return 2;
    }
    if (argc >= 3 && !parse_double_arg(argv[2], 0.002, 0.05, &cfg.dt_sec)) {
        fprintf(stderr, "Invalid dtSec: %s (expected 0.002..0.05)\n", argv[2]);
        return 2;
    }
    if (argc >= 4 && !parse_double_arg(argv[3], 0.01, 0.95, &cfg.line_threshold)) {
        fprintf(stderr, "Invalid lineThreshold: %s (expected 0.01..0.95)\n", argv[3]);
        return 2;
    }
    if (argc >= 5) {
        cfg.report_path = argv[4];
    }
    if (argc >= 6 && !parse_u32_arg(argv[5], &cfg.base_seed)) {
        fprintf(stderr, "Invalid baseSeed: %s\n", argv[5]);
        return 2;
    }

    for (i = 0U; i < scenario_count; ++i) {
        results[i] = run_scenario(&scenarios[i], &cfg);

        if (!results[i].has_runtime_error) {
            completed += 1U;
        }

        if (results[i].longest_lost_sec > max_longest) {
            max_longest = results[i].longest_lost_sec;
        }

        score_sum += results[i].score;
        detect_sum += results[i].line_detection_rate;

        if (results[i].has_runtime_error) {
            if (runtime_issue_count > 0U) {
                strncat(runtime_ids, ", ", sizeof(runtime_ids) - strlen(runtime_ids) - 1U);
            }
            strncat(runtime_ids, results[i].id, sizeof(runtime_ids) - strlen(runtime_ids) - 1U);
            runtime_issue_count += 1U;
        }

        if (results[i].score < 70.0) {
            low_score_count += 1U;
        }

        printf("[%zu/%zu] %-20s score=%6.2f detect=%6.2f%% longestLost=%6.3fs sat=%6.2f%%\n",
               i + 1U,
               scenario_count,
               results[i].id,
               results[i].score,
               results[i].line_detection_rate * 100.0,
               results[i].longest_lost_sec,
               results[i].motor_saturation_rate * 100.0);
    }

    summary.scenario_count = (uint32_t)scenario_count;
    summary.completed_count = completed;
    summary.aborted = false;
    summary.overall_score = score_sum / (double)scenario_count;
    summary.avg_line_detection_rate = detect_sum / (double)scenario_count;
    summary.max_longest_lost_sec = max_longest;

    if (runtime_issue_count > 0U) {
        static char runtime_issue[320];
        snprintf(runtime_issue, sizeof(runtime_issue), "Runtime errors in: %s", runtime_ids);
        issues[issue_count++] = runtime_issue;
    }
    if (summary.avg_line_detection_rate < 0.94) {
        issues[issue_count++] = "Average line detection is below 94%; tracking robustness is insufficient.";
    }
    if (summary.max_longest_lost_sec > 0.35) {
        issues[issue_count++] = "Longest line loss exceeds 0.35s in at least one scenario.";
    }
    if (summary.overall_score < 82.0) {
        issues[issue_count++] = "Overall score is below 82; tune speed/PID/recovery for stable behavior.";
    }
    if (low_score_count > 0U) {
        issues[issue_count++] = "At least one scenario score is below 70; local weakness remains.";
    }

    if (!write_report_json(cfg.report_path, &cfg, results, scenario_count, &summary, issues, issue_count)) {
        fprintf(stderr, "Failed to write report: %s\n", cfg.report_path);
        return 2;
    }

    printf("\nAuto test done: overall score %.2f/100, avg detection %.2f%%, max lost %.3fs\n",
           summary.overall_score,
           summary.avg_line_detection_rate * 100.0,
           summary.max_longest_lost_sec);
    printf("Report: %s\n", cfg.report_path);

    if (issue_count > 0U) {
        size_t k;
        printf("Issues:\n");
        for (k = 0U; k < issue_count; ++k) {
            printf("- %s\n", issues[k]);
        }
        return 1;
    }

    return 0;
}

void LF_Platform_BoardInit(void)
{
    g_left_cmd = 0;
    g_right_cmd = 0;
    g_led_on = false;
}

uint32_t LF_Platform_GetMillis(void)
{
    return (uint32_t)llround(g_sim_time_sec * 1000.0);
}

void LF_Platform_DelayMs(uint32_t ms)
{
    g_sim_time_sec += ((double)ms) / 1000.0;
}

void LF_Platform_ReadLineSensorRaw(uint16_t out_raw[LF_SENSOR_COUNT])
{
    double sensor_norm[LF_SENSOR_COUNT];

    if (out_raw == NULL) {
        return;
    }

    read_sensor_norm_and_raw(sensor_norm, out_raw);
}

void LF_Platform_SetMotorCommand(int16_t left_cmd, int16_t right_cmd)
{
    g_left_cmd = left_cmd;
    g_right_cmd = right_cmd;
}

void LF_Platform_SetStatusLed(bool on)
{
    g_led_on = on;
}

bool LF_Platform_IsStartButtonPressed(void)
{
    return false;
}

void LF_Platform_DebugPrint(const char *msg)
{
    (void)msg;
}
