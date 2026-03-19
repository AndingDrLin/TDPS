#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

DURATION_SEC="${1:-15}"
DT_SEC="${2:-0.01}"
LINE_THRESHOLD="${3:-0.12}"
SEED_START="${4:-20260319}"
RUNS="${5:-8}"
REPORT_DIR="${6:-${ROOT_DIR}/code/line_follow_v1/tests/reports/stability_runs}"

mkdir -p "${REPORT_DIR}"

min_score=101
min_detect=101
max_lost=0

for ((i=0; i<RUNS; i++)); do
  seed=$((SEED_START + i))
  report_path="${REPORT_DIR}/report_seed_${seed}.json"
  log_path="${REPORT_DIR}/run_seed_${seed}.log"

  echo "===== stability run $((i+1))/${RUNS} seed=${seed} ====="

  set +e
  bash "${ROOT_DIR}/scripts/run_line_follow_autotest.sh" \
    "${DURATION_SEC}" \
    "${DT_SEC}" \
    "${LINE_THRESHOLD}" \
    "${report_path}" \
    "${seed}" \
    >"${log_path}" 2>&1
  ec=$?
  set -e

  tail -n 6 "${log_path}"

  if [[ ${ec} -ne 0 ]]; then
    echo "stability check failed at seed=${seed}, see ${log_path}"
    exit ${ec}
  fi

  score=$(awk '/Auto test done: overall score/{v=$6; gsub(/\/100,/,"",v); print v; exit}' "${log_path}")
  detect=$(awk '/Auto test done: overall score/{v=$9; gsub(/%,/,"",v); print v; exit}' "${log_path}")
  lost=$(awk '/Auto test done: overall score/{v=$12; gsub(/s/,"",v); print v; exit}' "${log_path}")

  if awk "BEGIN{exit !(${score} < ${min_score})}"; then min_score=${score}; fi
  if awk "BEGIN{exit !(${detect} < ${min_detect})}"; then min_detect=${detect}; fi
  if awk "BEGIN{exit !(${lost} > ${max_lost})}"; then max_lost=${lost}; fi

done

echo "===== stability summary ====="
printf 'runs=%s min_score=%.2f min_detect=%.2f%% max_lost=%.3fs\n' "${RUNS}" "${min_score}" "${min_detect}" "${max_lost}"
echo "reports: ${REPORT_DIR}"
