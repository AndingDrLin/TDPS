# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language

Always respond in Chinese (中文). 每次回答开头都要说我已经查看了CLAUDE.md

## Repository layout

The main project lives under `TDPS/`; the repository root is a workspace wrapper.

- `TDPS/firmware/`: STM32F407VET6 firmware. Core logic is written so it can build both for STM32 HAL and for PC stub tests.
- `TDPS/simulator/`: offline simulation and regression harness for line following, radar, wireless, and integrated scenarios.
- `TDPS/tests/`: board-level and standalone peripheral tests.
- `TDPS/agent_docs/`: project brief, hardware reference, current progress, and agent working notes. Read these before non-trivial firmware changes.
- `TDPS/docs/`: design, line following, wireless, tuning, and testing documentation.
- `TDPS/docs_reference/`: local index for hardware/vendor references; large PDFs, archives, tools, images, and PCB exports are intentionally not committed.
- `TDPS/reports/`: report and experiment-record scripts.

## Naming conventions

- Directories and new management files use `snake_case`.
- Vendor reference files keep their original names to avoid path breakage.
- Generated outputs go to `firmware/build/`, `simulator/artifacts/`, `reports/*/outputs/`; default is not committed.

## Common commands

Run these from the repository root unless noted otherwise.

### Firmware PC stub tests with gcc

```bash
mkdir -p TDPS/firmware/build/gcc

gcc -ITDPS/firmware/Inc -ITDPS/firmware/common -ITDPS/firmware/platform \
    TDPS/firmware/Src/{lf_app,lf_control,lf_sensor,lf_radar,lf_chassis,lf_config,lf_config_profiles,lf_line_features,lf_speed_math,lf_platform_stub,lf_debug_monitor,lf_future_hooks,lf_led_blink,lf_run_log,wireless_hooks,wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    TDPS/firmware/test/test_lf_stub.c -o TDPS/firmware/build/gcc/lf_test -lm
./TDPS/firmware/build/gcc/lf_test

gcc -ITDPS/firmware/Inc -ITDPS/firmware/common -ITDPS/firmware/platform \
    TDPS/firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    TDPS/firmware/test/test_wl_stub.c -o TDPS/firmware/build/gcc/wl_test
./TDPS/firmware/build/gcc/wl_test
```

### Firmware CMake tests

CMake is optional in this repo; when installed, use:

```bash
cmake -S TDPS/firmware -B TDPS/firmware/build -DTDPS_TARGET_MCU=OFF -DTDPS_BUILD_TESTS=ON
cmake --build TDPS/firmware/build
ctest --test-dir TDPS/firmware/build --output-on-failure
```

Run a single CMake test:

```bash
ctest --test-dir TDPS/firmware/build -R test_lf --output-on-failure
ctest --test-dir TDPS/firmware/build -R test_wl --output-on-failure
```

Build with optional debug/bench definitions:

```bash
cmake -S TDPS/firmware -B TDPS/firmware/build \
  -DTDPS_TARGET_MCU=OFF -DTDPS_BUILD_TESTS=ON \
  -DTDPS_NO_CAR_MODE=ON -DTDPS_DEBUG_MONITOR=ON
```

CMake options: `TDPS_TARGET_MCU` (ON for STM32 HAL cross-build, OFF for PC stubs), `TDPS_BUILD_TESTS` (build test_lf/test_wl), `TDPS_NO_CAR_MODE` (bench debug), `TDPS_DEBUG_MONITOR` (periodic debug output).

### Simulator regression commands

```bash
bash TDPS/simulator/scripts/line_follow_cli.sh quick
bash TDPS/simulator/scripts/run_line_follow_stability.sh
bash TDPS/simulator/scripts/run_radar_autotest.sh
bash TDPS/simulator/scripts/run_wireless_autotest.sh
bash TDPS/simulator/scripts/run_system_autotest.sh
```

Quick gate criteria: `overallScore >= 82`, `avgLineDetectionRate >= 0.94`, `maxLongestLostSec <= 0.35`, no scenario `score < 70`.

Full-course stress run:

```bash
bash TDPS/simulator/scripts/line_follow_cli.sh full-course \
  TDPS/simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS/simulator/artifacts/line_follow_v1/reports/single_run/full_course_stress.json \
  20260319 stress
```

Some older testing docs still mention `TDPS-Simulator/`; current code is under `TDPS/simulator/`. If a script still assumes the old path, read `TDPS/agent_docs/03_agent_working_notes.md` and inspect the script before changing paths.

## Architecture notes

The firmware follows a `main loop + peripheral interrupts + state machines` model, with no RTOS. Core control logic must remain testable through PC stubs and simulator harnesses.

Firmware layers:

- Platform layer: `lf_platform_*`, `wl_platform_*`, UART callbacks, and board/HAL adapters isolate STM32 peripherals from portable logic. Two implementations: STM32F4 HAL (real hardware) and stubs (PC testing).
- Algorithm layer: `lf_sensor`, `lf_control`, and `lf_chassis` process 8-channel line input, run PID, and produce differential drive commands.
- Application layer: `lf_app` owns the line-following state machine: `WAIT_START -> CALIBRATING -> RUNNING -> RECOVERING / AVOID_* / FORK_* -> STOPPED / FAULT`.
- Extension layer: `wireless_hooks`, `lf_future_hooks`, `wl_app`, `wl_lora`, and `lf_radar` integrate LoRa checkpoints and radar obstacle states through non-blocking ticks.

### Current control architecture

6-parameter simplified PD + curvature feedforward (kff), toggled by `TDPS_SIMPLE_CONTROL=1` in `lf_app.c:1`:

- Core formula: `correction = kp*error + kd*derivative + kff*derivative*speed`
- Speed function: `speed = base_speed - (base_speed - min_speed) * |error| / 1750` (IIR smoothing alpha=0.4)
- Parameters: `kp=0.25`, `kd=1.20`, `kff=0.0008`, `base_speed=280`, `min_speed=60`, `max_correction=300`
- Disabled features: dead zone, soft zone, integral, speed scaling, rate limiting, double filtering, straight_boost, curve_prepare
- To revert to old 62-parameter architecture: set `TDPS_SIMPLE_CONTROL=0`

### Segment-based control (新增 2026-06-04)

分段控制系统，通过 `segment_control_enable`（debug profile 默认 true）启用：

**路段类型** (`LF_SegmentType` 枚举，定义在 `lf_config.h`)：
- `LF_SEGMENT_STRAIGHT` — 直道：2-3通道活跃，|pos|小且稳定
- `LF_SEGMENT_GENTLE_CURVE` — 缓弯：position逐渐偏移，一侧活跃递增
- `LF_SEGMENT_TIGHT_CURVE` — 急弯/连续弯：position大幅偏移，active>=4
- `LF_SEGMENT_WIDE_LINE` — 宽线/路口/直角弯入口：大面积亮(active>=6, sum>=3500)
- `LF_SEGMENT_FORK` — 岔路口：复用 `frame_looks_like_fork()` 条件
- `LF_SEGMENT_LOST` — 丢线中

**路段检测** (`detect_segment_type()` in `lf_app.c`)：基于传感器特征+时序历史，滞回机制（3帧确认+8帧保持防抖动）。

**分段参数**：每种路段独立 kp/kd/kff/base_speed/min_speed/max_correction（6段×6参数=36个新参数，前缀 `seg_*`）。切换时 IIR 平滑过渡（alpha=0.5）。

**连续弯增强**：维护最近20帧方向切换环形缓冲。连续弯中：grace_ticks 扩展(+4帧)、降速(80)转向、grace 用尽后1帧急转向（correction×1.3）。

**直角转弯增强**：
- `detect_right_angle_turn_side()` 放宽检测：active_count≥4（原≥5），新增次级条件（一侧≥1暗+另一侧≤1暗）
- 新增 `LF_APP_STATE_REORIENT_APPROACH`：低速前进靠近弯点后再旋转
- 旋转超时后倒车重试（`reorient_backtrack_enable`，最多 `reorient_max_retries` 次，每次反转方向）
- 调试输出增加 `seg=<类型> seg_sw=<方向切换计数> retry=<重试次数>`

**关键配置开关**：
- `segment_control_enable` — 分段控制总开关（debug profile: true, competition profile: inherited from debug）
- `seg_curve_direction_switch_min` — 连续弯方向切换阈值（默认2）
- `reorient_backtrack_enable` — 直角弯旋转超时后倒车重试（默认true）
- `reorient_max_retries` — 直角弯重试最大次数（默认2）

### Geometric constraints

当前车型为**倒三轮**（两前驱主动轮 + 后万向轮），传感器阵列中心到前驱动轮轴线中点约 **4 cm**，左右驱动轮中心距约 16 cm。传感器前置距离极短（4 cm vs 正三轮 22 cm），导致：
- 入弯检测窗口极短（传感器看到特殊线型时，驱动轮轴几乎同时到达）
- `lead_compensation` 前探补偿意义不大（已关闭）
- 急弯/直角转弯必须依赖"停车+原地旋转对准"策略（`reorient_*` 参数）
- PID 参数 kp/kd 需比正三轮低（短前探 = 高频振荡风险更大）

当处理路口、U 型顶点、直角转弯和圆形入口时，以驱动轮轴线中点作为转向参考点。

### Project state

The firmware startup path currently uses `LF_Config_ApplyDebugProfile()` by default. Use the low-speed conservative debug profile unless the user explicitly asks to switch. The debug profile disables fork detection; only the competition/track profile enables it.

## Key constraints from the design docs

- Do not block the line-following control loop. LoRa sending and radar parsing are advanced via non-blocking tick functions.
- Tuning parameters should usually be centralized in `TDPS/firmware/Src/lf_config.c` and profile overrides in `TDPS/firmware/Src/lf_config_profiles.c` rather than scattered through algorithms.
- `LF_App_NotifyCheckpoint(checkpoint_id)` is the integration entry for checkpoint events.
- LoRa reports use `TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<MM:SS>\n`; current defaults are `TEAM=15`, `NAME=TDPS`.
- Radar currently provides forward distance/state (`CLEAR/WARN/BLOCK`). It does not provide true left/right obstacle localization; current avoidance direction comes from config, last line direction, and reverse retry.
- Track profile uses Yahboom 8-LP UART line sensor input with polarity inversion and fast calibration; verify this before changing sensor mode or weights.
- Avoidance is timed open-loop (`AVOID_PREP -> TURN_OUT -> BYPASS -> TURN_IN -> REACQUIRE`) and must be retuned after hardware, battery, motor, floor, or load changes.

Before changing firmware behavior, read:

1. `TDPS/agent_docs/00_project_brief.md`
2. `TDPS/agent_docs/01_hardware_reference.md`
3. `TDPS/agent_docs/02_current_progress.md`
4. Relevant design docs under `TDPS/docs/`, especially `basic_design.md`, `line_following.md`, `tuning.md`, and `wireless_comm.md`.

## Board and hardware testing

Board-level test notes live at `TDPS/tests/board/tdps_board_test/README.md`. The recommended current path is ST-Link/SWD plus Keil Watch on `g_board_swd`; USART6 CLI is documented for later use with USB-TTL.

Important board-test safety detail from the docs: before motor testing, raise the wheels, use current-limited power, start with low duty cycle, and stop immediately with `command = 12` or reset/power-off if anything is abnormal.

## Generated files and large references

Do not commit generated outputs from `TDPS/firmware/build/`, `TDPS/simulator/artifacts/`, or `TDPS/reports/*/outputs/`. Do not add local vendor PDFs, archives, executables, images, PCB exports, or large tool packages from `TDPS/docs_reference/`; keep that directory as an index unless a small text summary is needed.
