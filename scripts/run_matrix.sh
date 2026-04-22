#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "run_matrix.sh is a thin wrapper over the suite runner."
echo "Running canonical suite: benchmark/spec/suites/community_v1.json"
echo

python3 -m benchmark.runner.run_suite \
  --suite benchmark/spec/suites/community_v1.json \
  --generate-manifests
