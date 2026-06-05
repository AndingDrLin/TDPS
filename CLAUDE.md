# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language

Always respond in Chinese (中文).

## Source of truth

The repository root is a workspace wrapper. The main project lives under `TDPS/`, and commands below assume the current working directory is the outer repository root: `/Users/linyujia/Project/TDPS`.

This root-level `CLAUDE.md` is the source of truth for Claude Code guidance. If `TDPS/CLAUDE.md` or older docs conflict with this file, follow this file and verify against current code before changing behavior. Do not create another Claude guidance file.

If Claude Code is launched from the inner `TDPS/` directory, remove the leading `TDPS/` path component from commands in this file.

## Repository layout

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
- Generated outputs go to `TDPS/firmware/build/`, `TDPS/simulator/artifacts/`, and `TDPS/reports/*/outputs/`; default is not committed.

## Common commands

Run these from the repository root unless noted otherwise.

### Firmware CMake tests

CMake is optional in this repo; when installed, prefer it over manual gcc commands because `TDPS/firmware/CMakeLists.txt` tracks current source files.

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

CMake options: `TDPS_TARGET_MCU` (ON for STM32 HAL cross-build, OFF for PC stubs), `TDPS_BUILD_TESTS` (build `test_lf`/`test_wl`), `TDPS_NO_CAR_MODE` (bench debug), `TDPS_DEBUG_MONITOR` (periodic debug output). In PC stub mode CMake also builds `wl_pc_serial_test`.

### Firmware PC stub tests with gcc

Use this as a quick smoke path when CMake is unavailable. If source files change, check `TDPS/firmware/CMakeLists.txt` before trusting this manual list.

```bash
mkdir -p TDPS/firmware/build/gcc

gcc -ITDPS/firmware/Inc -ITDPS/firmware/common -ITDPS/firmware/platform \
    TDPS/firmware/Src/{lf_app,lf_chassis,lf_config,lf_config_profiles,lf_control,lf_debug_monitor,lf_dgus_screen,lf_future_hooks,lf_radar,lf_sensor,lf_watch_debug,wireless_hooks,wl_app,wl_config,wl_lora,wl_protocol,lf_platform_stub,wl_platform_stub}.c \
    TDPS/firmware/test/test_lf_stub.c -o TDPS/firmware/build/gcc/lf_test -lm
./TDPS/firmware/build/gcc/lf_test

gcc -ITDPS/firmware/Inc -ITDPS/firmware/common -ITDPS/firmware/platform \
    TDPS/firmware/Src/{wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
    TDPS/firmware/test/test_wl_stub.c -o TDPS/firmware/build/gcc/wl_test
./TDPS/firmware/build/gcc/wl_test
```

### Simulator regression commands

```bash
bash TDPS/simulator/scripts/line_follow_cli.sh quick
bash TDPS/simulator/scripts/run_line_follow_stability.sh
bash TDPS/simulator/scripts/run_radar_autotest.sh
bash TDPS/simulator/scripts/run_wireless_autotest.sh
bash TDPS/simulator/scripts/run_system_autotest.sh
```

Quick gate criteria: `overallScore >= 82`, `avgLineDetectionRate >= 0.94`, `maxLongestLostSec <= 0.35`, no runtime error, and no scenario `score < 70`.

Full-course stress run:

```bash
bash TDPS/simulator/scripts/line_follow_cli.sh full-course \
  TDPS/simulator/sim_tests/line_follow_v1/config/scenarios_full_course.csv \
  90 0.01 0.12 \
  TDPS/simulator/artifacts/line_follow_v1/reports/single_run/full_course_stress.json \
  20260319 stress
```

Build the line-follow simulator runner directly:

```bash
bash TDPS/simulator/scripts/build_line_follow_runner.sh
```

Run parameter sweeps:

```bash
python TDPS/simulator/scripts/run_line_follow_param_sweep.py --mode coarse
python TDPS/simulator/scripts/run_line_follow_param_sweep.py --mode refine
python TDPS/simulator/scripts/run_line_follow_param_sweep.py --mode validate
```

Sweep overrides are simulator-only unless explicitly written back to firmware config: `TDPS_SIM_KP`, `TDPS_SIM_KD`, `TDPS_SIM_KFF`, `TDPS_SIM_BASE_SPEED`, `TDPS_SIM_MIN_SPEED`, `TDPS_SIM_MAX_CORRECTION`.

Some older testing docs still mention `TDPS-Simulator/`; current code is under `TDPS/simulator/`. If a script still assumes the old path, read `TDPS/agent_docs/03_agent_working_notes.md` and inspect the script before changing paths.

### Wireless PC serial test

```bash
bash TDPS/simulator/scripts/run_wireless_pc_serial.sh
```

### Report scripts

```bash
python TDPS/reports/mid_reports/scripts/build_all.py
python TDPS/reports/notebook/scripts/build_all.py
```

For the final report, run LaTeX from `TDPS/reports/final_report/src`:

```bash
pdflatex main.tex
pdflatex main.tex
```

## Firmware architecture

The firmware follows a `main loop + peripheral interrupts + state machines` model, with no RTOS. Core control logic must remain testable through PC stubs and simulator harnesses.

Firmware layers:

- Platform layer: `lf_platform_*`, `wl_platform_*`, UART callbacks, and board/HAL adapters isolate STM32 peripherals from portable logic. Two implementations exist: STM32F4 HAL for real hardware and stubs for PC testing.
- Algorithm layer: `lf_sensor`, `lf_control`, `lf_chassis`, `lf_radar`, and related helpers process 8-channel line input, radar frames, and differential drive commands.
- Application layer: `lf_app` owns the line-following state machine: `WAIT_START -> CALIBRATING -> RUNNING -> RECOVERING / AVOID_* / FORK_* / REORIENT_* -> STOPPED / FAULT`.
- Extension layer: `wireless_hooks`, `lf_future_hooks`, `wl_app`, `wl_lora`, debug monitor, run log, and LEDs integrate LoRa checkpoints, diagnostics, and radar obstacle states through non-blocking ticks.

Important entry points and integration rules:

- `LF_App_RunStep()` advances the line-following state machine.
- `Wireless_Hooks_Tick()` and `WL_LoRa_Tick()` advance LoRa work asynchronously.
- `LF_DebugMonitor_Tick()` emits periodic diagnostics when enabled.
- `LF_App_NotifyCheckpoint(checkpoint_id)` is the integration entry for checkpoint events.
- Do not block the line-following control loop. LoRa sending, radar parsing, debug output, and UI updates must advance through non-blocking tick functions.
- Do not do heavy work or blocking I/O in UART callbacks or interrupt paths.

Tuning parameters should usually be centralized in `TDPS/firmware/Src/lf_config.c` and profile overrides in `TDPS/firmware/Src/lf_config_profiles.c` rather than scattered through algorithms.

## Current control architecture

The default control path is a 6-parameter simplified PD + curvature feedforward (`kff`), toggled by `TDPS_SIMPLE_CONTROL=1` in `TDPS/firmware/Src/lf_app.c`:

- Core formula: `correction = kp*error + kd*derivative + kff*derivative*speed`
- Speed function: `speed = base_speed - (base_speed - min_speed) * |error| / 1750` with IIR smoothing alpha around 0.4
- Current core parameters from simulator scans: `kp=0.25`, `kd=1.20`, `kff=0.0008`, `base_speed=280`, `min_speed=60`, `max_correction=300`
- Disabled in simplified mode: dead zone, soft zone, integral, speed scaling, rate limiting, double filtering, straight boost, and curve prepare
- To revert to the old 62-parameter architecture, set `TDPS_SIMPLE_CONTROL=0`

### Segment-based control

Segment control is enabled through `segment_control_enable` in the config profile. `LF_SegmentType` is defined in `TDPS/firmware/Inc/lf_config.h`.

Segment types:

- `LF_SEGMENT_STRAIGHT`: straight line; usually 2-3 active channels and small stable position.
- `LF_SEGMENT_GENTLE_CURVE`: gentle curve; position gradually offsets and one side becomes more active.
- `LF_SEGMENT_TIGHT_CURVE`: tight/continuous curve; large position offset or active channel count around 4+.
- `LF_SEGMENT_WIDE_LINE`: wide line/intersection/right-angle entry; large bright area.
- `LF_SEGMENT_FORK`: fork; reuses `frame_looks_like_fork()` conditions.
- `LF_SEGMENT_LOST`: lost line.

`detect_segment_type()` in `TDPS/firmware/Src/lf_app.c` uses sensor features plus temporal history, with confirmation and hold frames for hysteresis. Segment parameters use `seg_*` fields for per-segment `kp/kd/kff/base_speed/min_speed/max_correction`, with IIR smoothing during transitions.

Continuous curve handling tracks recent direction switches. Right-angle handling includes `LF_APP_STATE_REORIENT_APPROACH`, `REORIENT_SPIN`, confirm logic, and optional backtrack retry through `reorient_backtrack_enable` and `reorient_max_retries`.

## Geometry-sensitive logic

Existing project docs have contained conflicting sensor-to-drive-axis distances, including approximately 4 cm and 22 cm. `TDPS/agent_docs/02_current_progress.md` currently records the geometry as aligned to the real car at about 22 cm sensor front distance and 16 cm wheel track, while an older root guidance snapshot described about 4 cm.

Before changing geometry-dependent logic such as `lead_advance_*`, `lead_compensation`, right-angle turns, U-turn apex handling, circular entries, or reorient timing, verify the latest hardware record, CAD/mechanical measurement, or board test notes. After verification, update affected docs and configs consistently instead of preserving both values.

For intersections, U-turn apexes, right-angle turns, and circular entries, reason about the midpoint of the left/right drive-wheel axis as the actual turning reference point.

## Project state and profiles

The firmware startup path currently uses `LF_Config_ApplyDebugProfile()` by default. Use the low-speed conservative debug profile unless the user explicitly asks to switch to competition/track behavior. The debug profile disables fork detection; only the competition/track profile enables it.

Track/debug sensor configuration currently uses Yahboom 8-LP UART line sensor input with polarity inversion and fast calibration. Verify sensor mode, polarity, channel order, and weights before changing sensor handling; this area is high risk for real-car regressions.

Before changing firmware behavior, read:

1. `TDPS/agent_docs/00_project_brief.md`
2. `TDPS/agent_docs/01_hardware_reference.md`
3. `TDPS/agent_docs/02_current_progress.md`
4. `TDPS/agent_docs/03_agent_working_notes.md`
5. Relevant design docs under `TDPS/docs/`, especially `basic_design.md`, `line_following.md`, `tuning.md`, and `wireless_comm.md`

## Wireless, radar, and checkpoint constraints

- LoRa reports use `TEAM=<id>,NAME=<name>,CP=<checkpoint>,TIME=<MM:SS>\n`; current defaults are `TEAM=15`, `NAME=TDPS`.
- LoRa sending is asynchronous with queueing, AUX readiness, retry, and optional ACK. Do not wait synchronously for radio completion in the control loop.
- Radar currently provides forward distance/state (`CLEAR/WARN/BLOCK`). It does not provide true left/right obstacle localization.
- Current avoidance direction comes from config, last line direction, and reverse retry, not from radar left/right perception.
- Avoidance is timed open-loop (`AVOID_PREP -> TURN_OUT -> BYPASS -> TURN_IN -> REACQUIRE`) and must be retuned after hardware, battery, motor, floor, or load changes.

## Verification expectations for behavior changes

For firmware control changes, run at least the relevant CMake tests and simulator quick regression. For line-following control, sensor weighting, segment transitions, speed profile, right-angle/reorient behavior, or checkpoint behavior, use:

```bash
ctest --test-dir TDPS/firmware/build -R test_lf --output-on-failure
bash TDPS/simulator/scripts/line_follow_cli.sh quick
bash TDPS/simulator/scripts/run_line_follow_stability.sh
bash TDPS/simulator/scripts/run_system_autotest.sh
```

For radar changes, also run:

```bash
bash TDPS/simulator/scripts/run_radar_autotest.sh
```

For wireless changes, also run:

```bash
ctest --test-dir TDPS/firmware/build -R test_wl --output-on-failure
bash TDPS/simulator/scripts/run_wireless_autotest.sh
bash TDPS/simulator/scripts/run_wireless_pc_serial.sh
```

If CMake has not been configured yet, run the CMake configure/build commands from the Common commands section first.

## Board and hardware testing

Board-level test notes live at `TDPS/tests/board/tdps_board_test/README.md`. The recommended current path is ST-Link/SWD plus Keil Watch on `g_board_swd`; USART6 CLI is documented for later use with USB-TTL.

Important board-test safety detail from the docs: before motor testing, raise the wheels, use current-limited power, start with low duty cycle, and stop immediately with `command = 12` or reset/power-off if anything is abnormal.

Main Keil project:

```text
TDPS/firmware/MDK-ARM/TDPS_LineFollow.uvprojx
```

Standalone peripheral test projects:

```text
TDPS/tests/standalone/line_sensor_uart_8plus2_test/MDK-ARM/line_sensor_uart_8plus2_test.uvprojx
TDPS/tests/standalone/radar_ld2410s/MDK-ARM/radar_ld2410s.uvprojx
TDPS/tests/standalone/lora_module_test/MDK-ARM/lora_module_test.uvprojx
```

## Generated files and large references

Do not commit generated outputs from `TDPS/firmware/build/`, `TDPS/simulator/artifacts/`, or `TDPS/reports/*/outputs/`. Do not add local vendor PDFs, archives, executables, images, PCB exports, or large tool packages from `TDPS/docs_reference/`; keep that directory as an index unless a small text summary is needed.
