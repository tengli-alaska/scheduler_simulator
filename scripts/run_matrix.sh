#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/bin/scheduler_sim"
RUNS_DIR="${ROOT_DIR}/runs"
LOG_DIR="${RUNS_DIR}/logs"

if [[ ! -x "${BIN}" ]]; then
  echo "error: binary not found at ${BIN}. Build first with: make" >&2
  exit 1
fi

mkdir -p "${RUNS_DIR}" "${LOG_DIR}"
find "${RUNS_DIR}" -maxdepth 1 -type f -name "*.csv" -delete
find "${LOG_DIR}" -type f -name "*.log" -delete 2>/dev/null || true

run_case() {
  local workload="$1"
  local tasks="$2"
  local stop_time="$3"
  local reps="${4:-1}"

  local tag="${workload}_n${tasks}_t${stop_time}_r${reps}"
  echo "==> ${tag} | c1 sq"
  "${BIN}" -s all -w "${workload}" -n "${tasks}" -c 1 -m sq -r "${reps}" -t "${stop_time}" \
    > "${LOG_DIR}/${tag}_c1_sq.log" 2>&1

  echo "==> ${tag} | c4 sq"
  "${BIN}" -s all -w "${workload}" -n "${tasks}" -c 4 -m sq -r "${reps}" -t "${stop_time}" \
    > "${LOG_DIR}/${tag}_c4_sq.log" 2>&1

  echo "==> ${tag} | c4 mq rr steal-on"
  "${BIN}" -s all -w "${workload}" -n "${tasks}" -c 4 -m mq -b rr -r "${reps}" -t "${stop_time}" \
    > "${LOG_DIR}/${tag}_c4_mq_rr_steal_on.log" 2>&1

  echo "==> ${tag} | c4 mq rr steal-off"
  "${BIN}" -s all -w "${workload}" -n "${tasks}" -c 4 -m mq -b rr --no-steal -r "${reps}" -t "${stop_time}" \
    > "${LOG_DIR}/${tag}_c4_mq_rr_steal_off.log" 2>&1
}

# Synthetic workloads (two load levels)
run_case server 1000 200000 3
run_case server 5000 500000 3
run_case desktop 1000 200000 3
run_case desktop 5000 500000 3

# Real traces (two load levels each, tuned to keep all schedulers quantifiable)
run_case alibaba 300 1000000 1
run_case alibaba 1000 1000000 1
run_case google 20 5000000 1
run_case google 40 5000000 1

echo
echo "Completed matrix runs. Output files:"
ls -1 "${RUNS_DIR}"/*.csv
