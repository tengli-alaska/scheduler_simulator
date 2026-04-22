# Benchmark Task Board (Lean)

This board tracks only active work needed to keep the tool usable as a benchmark framework.

## Current Baseline

- Suite-driven execution via `python3 -m benchmark.runner.run_suite`
- Isolated artifacts in `runs/<suite_id>/<run_id>/`
- Canonical analysis outputs (`metrics_enriched.csv`, `run_index.csv`, `quality_checks.csv`)
- Standardized experiment plots via `scripts/plot_experiment_suite.py`
- Optional markdown report via `python3 -m benchmark.report --suite <suite> --format md`

## Active Backlog

1. Add smoke CI run for a tiny suite to verify runner + aggregation contracts.
2. Add deterministic publish profile (`replications >= 5`) and document seed policy.
3. Add one end-to-end tutorial suite focused on quick local validation.
4. Add a lightweight schema drift check in CI for analysis output headers.

## Done Criteria for v1

1. New researchers can run one command and produce run artifacts, analysis CSVs, and plots.
2. Adding a benchmark requires only one suite file and optional plugin registration.
3. Failures are diagnosable using run logs + `analysis/run_index.csv` + `analysis/quality_checks.csv`.
