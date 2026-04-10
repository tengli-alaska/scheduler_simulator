#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"
make -j4
./scripts/run_matrix.sh
./scripts/aggregate_results.py

echo
echo "Full analysis pipeline complete."
echo "Runs:     ${ROOT_DIR}/runs"
echo "Analysis: ${ROOT_DIR}/analysis"
