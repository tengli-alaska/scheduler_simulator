# Benchmarking Quickstart (Lean v1)

This guide focuses on the shortest path to run reproducible benchmarks.

## Prerequisites

1. Build simulator:

```bash
make -j4
```

2. (Optional for plots) install plotting deps:

```bash
python3 -m pip install -r scripts/plot_requirements.txt
```

## One-command pipeline

Run the full benchmark workflow for the default suite:

```bash
./scripts/run_full_analysis.sh
```

Run for a custom suite JSON:

```bash
./scripts/run_full_analysis.sh benchmark/spec/suites/community_v1.json
```

Optional rich report pack (off by default):

```bash
BENCHMARK_ENABLE_REPORT=1 ./scripts/run_full_analysis.sh
```

## Core outputs you should look at first

- `runs/<suite_id>/<run_id>/` (raw artifacts + manifest)
- `analysis/metrics_enriched.csv`
- `analysis/run_index.csv`
- `analysis/quality_checks.csv`
- `analysis/summary_table.csv`
- `figures/experiments/<suite_id>/`

## Debugging failed runs

1. Check runner logs:
   - `runs/logs/suite_runner/*.log`
2. Check run index lineage:
   - `analysis/run_index.csv`
3. Check quality warnings:
   - `analysis/quality_checks.csv`

## Minimal manual flow (if you do not want the wrapper)

```bash
python3 scripts/validate_benchmark_spec.py --suite benchmark/spec/suites/community_v1.json
python3 -m benchmark.runner.run_suite --suite benchmark/spec/suites/community_v1.json --generate-manifests
python3 scripts/aggregate_results.py
python3 scripts/export_summary_table.py
python3 scripts/plot_experiment_suite.py --analysis-dir analysis --output-dir figures/experiments/community-core-exp1-exp6 --suite benchmark/spec/suites/community_v1.json
```
