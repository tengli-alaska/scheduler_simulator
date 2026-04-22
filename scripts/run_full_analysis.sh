#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUITE_PATH="${1:-benchmark/spec/suites/community_v1.json}"
BENCHMARK_ENABLE_REPORT="${BENCHMARK_ENABLE_REPORT:-0}"

cd "${ROOT_DIR}"
python3 scripts/validate_benchmark_spec.py --suite "${SUITE_PATH}"

SUITE_ID="$(python3 - "${SUITE_PATH}" <<'PY'
import json, sys
from pathlib import Path
p = Path(sys.argv[1]).resolve()
with p.open("r", encoding="utf-8") as f:
    doc = json.load(f)
print(doc.get("suite_id", "suite"))
PY
)"

make -j4
python3 -m benchmark.runner.run_suite \
  --suite "${SUITE_PATH}" \
  --clean-suite-dir \
  --generate-manifests
python3 scripts/aggregate_results.py
python3 scripts/export_summary_table.py --analysis-dir analysis

if python3 -c "import pandas, seaborn, matplotlib, numpy" >/dev/null 2>&1; then
  python3 scripts/plot_experiment_suite.py \
    --analysis-dir analysis \
    --output-dir "figures/experiments/${SUITE_ID}" \
    --formats png,pdf \
    --dpi 300 \
    --suite "${SUITE_PATH}"

  if [ "${BENCHMARK_ENABLE_REPORT}" = "1" ]; then
    python3 -m benchmark.report --suite "${SUITE_PATH}" --format md
  else
    echo "[info] Skipping report-pack generation (set BENCHMARK_ENABLE_REPORT=1 to enable)."
  fi
else
  echo "[warn] Skipping plots/report (missing pandas/seaborn/matplotlib/numpy)."
fi

echo
echo "Benchmark pipeline complete."
echo "Suite:    ${SUITE_ID}"
echo "Runs:     ${ROOT_DIR}/runs"
echo "Analysis: ${ROOT_DIR}/analysis"
echo "Summary:  ${ROOT_DIR}/analysis/summary_table.csv"
echo "Figures:  ${ROOT_DIR}/figures/experiments/${SUITE_ID}"
echo "Reports:  ${ROOT_DIR}/reports"
