#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

DURATION_SEC="${1:-15}"
DT_SEC="${2:-0.01}"
LINE_THRESHOLD="${3:-0.12}"
REPORT_PATH="${4:-${ROOT_DIR}/code/line_follow_v1/tests/reports/single_run/last_autotest_report.json}"
BASE_SEED="${5:-20260319}"
RUNNER_BIN="${ROOT_DIR}/code/line_follow_v1/tests/.lf_autotest_runner"

mkdir -p "$(dirname "${REPORT_PATH}")"

echo "[autotest] building harness..."
gcc -std=c11 -Wall -Wextra -Werror \
  -I"${ROOT_DIR}/code/line_follow_v1/Inc" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_app.c" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_chassis.c" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_config.c" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_control.c" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_sensor.c" \
  "${ROOT_DIR}/code/line_follow_v1/Src/lf_future_hooks.c" \
  "${ROOT_DIR}/code/line_follow_v1/tests/lf_autotest_harness.c" \
  -lm \
  -o "${RUNNER_BIN}"

echo "[autotest] running suite (duration=${DURATION_SEC}s, dt=${DT_SEC}s, threshold=${LINE_THRESHOLD}, seed=${BASE_SEED})..."
set +e
"${RUNNER_BIN}" "${DURATION_SEC}" "${DT_SEC}" "${LINE_THRESHOLD}" "${REPORT_PATH}" "${BASE_SEED}"
EXIT_CODE=$?
set -e

echo "[autotest] report: ${REPORT_PATH}"
exit ${EXIT_CODE}
