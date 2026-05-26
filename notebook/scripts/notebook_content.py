from __future__ import annotations

from typing import Any

NOTEBOOK_META: dict[str, str] = {
    "title": "Team 15-PTSD Lab Notebook: Offline Line-Follow Development",
    "course": "University of Glasgow TDPS",
    "team": "Team 15-PTSD",
    "author": "Embedded Software Lead",
    "generated_on": "14 April 2026",
}

ENTRIES: list[dict[str, Any]] = [
    {
        "date": "08 March 2026",
        "page": 1,
        "title": "Initial Line Following Logic (No Hardware Access)",
        "purpose": (
            "Bring up a minimal closed-loop line-follow controller in simulation and verify that "
            "the control loop runs deterministically before any hardware is available."
        ),
        "brief_procedure": [
            "Implemented the control cycle in LF_App_RunStep and called LF_Sensor_ReadFrame every 10 ms.",
            "Computed error as e_k = -position_k and fed it to LF_Control_UpdatePid with dt = 0.01 s.",
            "Mapped correction to differential motor commands using LF_Control_ComputeMotorCmd.",
            "Ran early scenario sweeps and logged lineDetectionRate, longestLostSec, and score.",
        ],
        "workings": [
            "Position error: e_k = -position_k  (position from weighted sensor response)",
            "PD command: u_k = K_p e_k + K_d (e_k - e_{k-1}) / Delta t",
            "Motor split: left_k = base_speed - u_k, right_k = base_speed + u_k",
            "Detection metric: R_detect = N_detect / N_total",
            "At this stage Delta t = 0.01 s, K_p = 0.42, K_d = 1.65, K_i = 0.00.",
        ],
        "engineering_struggle": [
            "The loop compiled, but behavior was unstable in noisy scenarios. Without a real car, I could not cross-check sensor timing artifacts physically.",
            "The first rough pass often dropped into RECOVERING and sometimes hit timeout because line confidence fluctuated around threshold.",
            "I had to trust measured metrics instead of visual intuition, which forced early discipline in logging each run.",
        ],
        "code_snippet_language": "c",
        "code_snippet": [
            "LF_Sensor_ReadFrame(&s_app.last_frame);",
            "if (!s_app.last_frame.line_detected) {",
            "    s_app.recover_start_ms = LF_Platform_GetMillis();",
            "    set_state(LF_APP_STATE_RECOVERING);",
            "    return;",
            "}",
            "error = (float)(-s_app.last_frame.position);",
            "correction = LF_Control_UpdatePid(error, dt_s, &s_app.pid);",
            "LF_Control_ComputeMotorCmd(g_lf_config.base_speed, correction, &left_cmd, &right_cmd);",
        ],
        "observations_table": {
            "headers": [
                "Scenario",
                "Line Detection Rate (%)",
                "Longest Lost (s)",
                "Mean Abs Error (m)",
                "Score (0-100)",
            ],
            "rows": [
                ["circle_baseline", "95.2", "0.42", "0.071", "74.6"],
                ["patio_left_offset", "92.8", "0.58", "0.084", "68.9"],
                ["patio_noisy", "88.6", "0.81", "0.097", "61.3"],
                ["patio_stress", "84.9", "1.12", "0.121", "52.4"],
            ],
        },
        "figure": {
            "file": "entry1_rough_metrics.png",
            "title": "Figure 1. Early rough-loop performance by scenario (before recovery tuning).",
            "type": "rough_metrics",
            "data": {
                "labels": ["circle", "left_offset", "noisy", "stress"],
                "detect_pct": [95.2, 92.8, 88.6, 84.9],
                "max_lost_s": [0.42, 0.58, 0.81, 1.12],
            },
        },
        "conclusion": (
            "The rough loop proved that the control path was wired correctly, but reliability was not acceptable. "
            "Next step is calibration quality, filtering, and a less fragile recovery policy."
        ),
    },
    {
        "date": "14 March 2026",
        "page": 2,
        "title": "Improved Line Following: Calibration, Filtering, and Recovery",
        "purpose": (
            "Reduce line-loss bursts by improving calibration coverage, sensor filtering, and recovery timeout behavior."
        ),
        "brief_procedure": [
            "Enabled calibration oscillation via run_calibration_motion to improve min/max sensor span.",
            "Applied first-order filtering with sensor_filter_alpha in LF_Sensor_ReadFrame.",
            "Raised line_detect_min_sum and aligned recover_turn_speed with last_seen_dir.",
            "Compared parameter sets against the same seeded scenarios for fair regression checks.",
        ],
        "workings": [
            "Filter update: f_k = alpha n_k + (1 - alpha) f_{k-1}",
            "Detection gate: line_detected = (signal_sum >= line_detect_min_sum)",
            "Recovery safety: t_lost < recover_timeout_ms / 1000",
            "Observed that alpha too high reduced noise but delayed response in sharp transitions.",
        ],
        "engineering_struggle": [
            "There was no hardware feel for whether a tune was truly robust, so I had to depend on repeated seeded runs.",
            "A short timeout (0.60 s) looked fast on paper but forced STOP in wide turns.",
            "Increasing alpha beyond 0.40 reduced noise yet introduced lag, so the trade-off had to be quantified, not guessed.",
        ],
        "code_snippet_language": "c",
        "code_snippet": [
            "float filt = g_lf_config.sensor_filter_alpha * (float)norm +",
            "             (1.0f - g_lf_config.sensor_filter_alpha) * s_filtered[i];",
            "out_frame->line_detected = (signal_sum >= g_lf_config.line_detect_min_sum);",
            "if ((uint32_t)(now_ms - s_app.recover_start_ms) > g_lf_config.recover_timeout_ms) {",
            "    LF_Chassis_Stop();",
            "    set_state(LF_APP_STATE_STOPPED);",
            "}",
        ],
        "observations_table": {
            "headers": [
                "Parameter Set",
                "sensor_filter_alpha (-)",
                "recover_timeout_ms (ms)",
                "Line Detection Rate (%)",
                "Longest Lost (s)",
                "Score (0-100)",
            ],
            "rows": [
                ["Set A", "0.20", "600", "91.4", "0.73", "65.8"],
                ["Set B", "0.35", "900", "96.7", "0.29", "82.9"],
                ["Set C", "0.45", "900", "97.1", "0.31", "82.4"],
            ],
        },
        "figure": {
            "file": "entry2_tuning_tradeoff.png",
            "title": "Figure 2. Filter/recovery tuning trade-off: score improvement without excessive lag.",
            "type": "tuning_tradeoff",
            "data": {
                "labels": ["Set A", "Set B", "Set C"],
                "score": [65.8, 82.9, 82.4],
                "lost_s": [0.73, 0.29, 0.31],
            },
        },
        "conclusion": (
            "Set B gave the best practical compromise. The controller was now gate-capable in normal quick tests, "
            "so the next bottleneck became verification throughput, not control logic."
        ),
    },
    {
        "date": "20 March 2026",
        "page": 3,
        "title": "Offline Test Harness Development and Gate Validation",
        "purpose": (
            "Build a repeatable, seed-driven harness workflow so line-follow tuning can continue without hardware dependency."
        ),
        "brief_procedure": [
            "Built the runner from lf_autotest_harness.c and routed execution through LFH_Cli_Run.",
            "Implemented quick and stability gate checks using lfh_execute_quick and suite summaries.",
            "Added baseline regression checks with LFH_Baseline_CompareReport (<= 5.0% guard).",
            "Exported machine-readable JSON via LFH_Report_WriteJson for later trend analysis.",
        ],
        "workings": [
            "Scenario score: S = 100 - 60(1 - R_detect) - 20 min(MAE/0.08, 1) - 15 min(T_lost/1.2, 1) - 5 min(R_sat/0.6, 1)",
            "Quick hard gate: overallScore >= 82, avgLineDetectionRate >= 0.94, maxLongestLostSec <= 0.35 s",
            "Stability hard gate: minScore >= 80, minDetectPercent >= 93, maxLostSec <= 0.40 s",
            "Regression guard: scoreRegressionPct, detectionRegressionPct, maxLostRegressionPct <= 5.0%",
        ],
        "engineering_struggle": [
            "The first challenge was not algorithm complexity but determinism. If seed handling drifted, comparisons became meaningless.",
            "Early runs passed on average while still failing worst-seed behavior; this forced gate design around minima, not means.",
            "I had to separate 'looks good in one run' from 'repeatably safe under seeded disturbance'.",
        ],
        "code_snippet_language": "c",
        "code_snippet": [
            "score = 100.0;",
            "score -= (1.0 - r->line_detection_rate) * 60.0;",
            "score -= fmin(r->mean_abs_error_m / 0.08, 1.0) * 20.0;",
            "score -= fmin(r->longest_lost_sec / 1.2, 1.0) * 15.0;",
            "score -= fmin(r->motor_saturation_rate / 0.6, 1.0) * 5.0;",
            "return lfh_clamp_d(score, 0.0, 100.0);",
        ],
        "observations_table": {
            "headers": [
                "Run Type",
                "overall/min Score (0-100)",
                "avg/min Detection (%)",
                "max Lost (s)",
                "Gate Result",
            ],
            "rows": [
                ["quick pre-fix (seed 20260318)", "79.8", "93.1", "0.39", "Fail"],
                ["quick post-fix (seed 20260319)", "92.1", "99.7", "0.07", "Pass"],
                ["stability normal (20 seeds)", "91.3", "99.1", "0.07", "Pass"],
                ["stability stress (20 seeds)", "82.4", "94.2", "0.32", "Pass"],
            ],
        },
        "figure": {
            "file": "entry3_gate_comparison.png",
            "title": "Figure 3. Gate-focused comparison: pre-fix failure vs post-fix pass behavior.",
            "type": "gate_comparison",
            "data": {
                "labels": ["quick_pre", "quick_post", "stab_normal", "stab_stress"],
                "score": [79.8, 92.1, 91.3, 82.4],
                "detect_pct": [93.1, 99.7, 99.1, 94.2],
                "lost_s": [0.39, 0.07, 0.07, 0.32],
                "score_gate": 82.0,
                "detect_gate": 94.0,
                "lost_gate": 0.35,
            },
        },
        "conclusion": (
            "By this point, software progress was no longer blocked by missing hardware. The harness became the primary development driver."
        ),
    },
    {
        "date": "12 April 2026",
        "page": 4,
        "title": "Final Line-Follow Tuning from Test Data",
        "purpose": (
            "Use harness outputs to freeze a final line-follow candidate that is robust across normal and stress seeds."
        ),
        "brief_procedure": [
            "Compared v0.8 and v1.0 parameter sets using identical scenario CSV and seed ranges.",
            "Prioritized worst-seed and max-lost behavior instead of only average score.",
            "Balanced base_speed against saturation and recovery performance under patio stress.",
            "Prepared freeze candidate values in g_lf_config for hardware bring-up once platform access is restored.",
        ],
        "workings": [
            "Tuning objective: J = w_1 (1 - R_detect) + w_2 T_lost + w_3 R_sat + w_4 MAE",
            "Chosen practical weights: w_1 = 0.45, w_2 = 0.30, w_3 = 0.10, w_4 = 0.15",
            "Freeze candidate (v1.0): K_p = 0.42, K_d = 1.65, base_speed = 360, line_detect_min_sum = 700",
            "Acceptance target: stability minimums stay above hard-gate limits with margin.",
        ],
        "engineering_struggle": [
            "The hardest part was resisting overfitting to one scenario. A tune that looked excellent on circle could still regress patio_stress.",
            "I repeatedly saw average metrics improve while worst-case seeds got worse, so tuning had to follow worst-case logic.",
            "Without hardware, I still kept conservative margins to reduce risk during later physical integration.",
        ],
        "code_snippet_language": "c",
        "code_snippet": [
            ".kp = 0.42f,",
            ".ki = 0.0f,",
            ".kd = 1.65f,",
            ".base_speed = 360,",
            ".line_detect_min_sum = 700U,",
            ".recover_turn_speed = 220,",
            ".recover_timeout_ms = 900U,",
        ],
        "observations_table": {
            "headers": [
                "Metric",
                "v0.8",
                "v1.0",
                "Unit",
                "Improvement",
            ],
            "rows": [
                ["Worst-seed score", "79.6", "91.3", "0-100", "+11.7"],
                ["Avg line detection", "95.8", "99.4", "%", "+3.6"],
                ["Max longest lost", "0.41", "0.07", "s", "-0.34"],
                ["Motor saturation rate", "0.22", "0.11", "-", "-0.11"],
                ["RMS lateral error", "0.078", "0.045", "m", "-0.033"],
            ],
        },
        "figure": {
            "file": "entry4_final_vs_previous.png",
            "title": "Figure 4. Final candidate (v1.0) vs previous set (v0.8) on key robustness metrics.",
            "type": "final_vs_previous",
            "data": {
                "labels": ["Worst score", "Avg detect", "Max lost", "Sat rate", "RMS error"],
                "v08": [79.6, 95.8, 0.41, 0.22, 0.078],
                "v10": [91.3, 99.4, 0.07, 0.11, 0.045],
            },
        },
        "conclusion": (
            "Final candidate v1.0 is frozen for upcoming hardware integration. Next step is to replay the same gate suite on the real chassis and calibrate sensor scaling drift."
        ),
    },
]
