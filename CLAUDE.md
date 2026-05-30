# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language

Always respond in Chinese (中文).

## Repository layout

The main project lives under `TDPS/`; the repository root is a workspace wrapper.

- `TDPS/firmware/`: STM32F407VET6 firmware. Core logic is written so it can build both for STM32 HAL and for PC stub tests.
- `TDPS/simulator/`: offline simulation and regression harness for line following, radar, wireless, and integrated scenarios.
- `TDPS/tests/`: board-level and standalone peripheral tests.
- `TDPS/agent_docs/`: project brief, hardware reference, current progress, and agent working notes. Read these before non-trivial firmware changes.
- `TDPS/docs/`: design, line following, wireless, tuning, and testing documentation.
- `TDPS/docs_reference/`: local index for hardware/vendor references; large PDFs, archives, tools, images, and PCB exports are intentionally not committed.
- `TDPS/reports/`: report and experiment-record scripts.

## Common commands

Run these from the repository root unless noted otherwise.

### Firmware PC stub tests with gcc

```bash
mkdir -p TDPS/firmware/build/gcc

gcc -ITDPS/firmware/Inc -ITDPS/firmware/common -ITDPS/firmware/platform \
    TDPS/firmware/Src/{lf_app,lf_control,lf_sensor,lf_radar,lf_chassis,lf_config,lf_platform_stub,lf_debug_monitor,lf_future_hooks,lf_led_blink,lf_run_log,wireless_hooks,wl_app,wl_lora,wl_protocol,wl_config,wl_platform_stub}.c \
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

### Simulator regression commands

```bash
bash TDPS/simulator/scripts/line_follow_cli.sh quick
bash TDPS/simulator/scripts/run_line_follow_stability.sh
bash TDPS/simulator/scripts/run_radar_autotest.sh
bash TDPS/simulator/scripts/run_wireless_autotest.sh
bash TDPS/simulator/scripts/run_system_autotest.sh
```

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

- Platform layer: `lf_platform_*`, `wl_platform_*`, UART callbacks, and board/HAL adapters isolate STM32 peripherals from portable logic.
- Algorithm layer: `lf_sensor`, `lf_control`, and `lf_chassis` process 8-channel line input, run PID, and produce differential drive commands.
- Application layer: `lf_app` owns the line-following state machine: `WAIT_START -> CALIBRATING -> RUNNING -> RECOVERING / AVOID_* -> STOPPED`.
- Extension layer: `wireless_hooks`, `lf_future_hooks`, `wl_app`, `wl_lora`, and `lf_radar` integrate LoRa checkpoints and radar obstacle states through non-blocking ticks.

Key constraints from the design docs:

- Do not block the line-following control loop. LoRa sending and radar parsing are advanced via non-blocking tick functions.
- Tuning parameters should usually be centralized in `TDPS/firmware/Src/lf_config.c` and profile overrides in `TDPS/firmware/Src/lf_config_profiles.c` rather than scattered through algorithms.
- The current real-car startup path calls `LF_Config_ApplyTrackProfile()`, not the low-speed debug profile. Track profile enables straight boost, curve preparation slowdown, line-stability filtering, stable direction recovery, and fork detection by default.
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
