# Add a New Benchmark in 10 Minutes

This is the fastest end-to-end path for a new researcher to add a benchmark suite.

## 1) Create a suite file

Start from:

- [`templates/new_suite.yaml`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/templates/new_suite.yaml)

Copy it and convert to JSON for the runner/validator, for example:

- `benchmark/spec/suites/my_suite_v1.json`

## 2) (Optional) Register a custom workload/scheduler key

If you need a new key or alias:

1. Copy [`templates/new_workload.py`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/templates/new_workload.py) or [`templates/new_scheduler.py`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/templates/new_scheduler.py) to:
   - `benchmark/plugins/local/my_workload.py`
2. Edit `key` and `cli_token`.

Reference:

- [`docs/plugin_registration.md`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/docs/plugin_registration.md)

## 3) Validate the suite

```bash
python3 scripts/validate_benchmark_spec.py --suite benchmark/spec/suites/my_suite_v1.json
```

## 4) Run the suite

```bash
python3 -m benchmark.runner.run_suite \
  --suite benchmark/spec/suites/my_suite_v1.json \
  --generate-manifests
```

## 5) Aggregate results

```bash
python3 scripts/aggregate_results.py
python3 scripts/export_summary_table.py
```

## 6) Build figures + report pack

```bash
python3 -m benchmark.report --suite my-suite-v1 --format md
```

Outputs:

- `figures/publication/my-suite-v1/`
- `reports/my-suite-v1/report.md`

## Minimal example

If your suite id is `my-suite-v1`, this single chain runs everything:

```bash
python3 scripts/validate_benchmark_spec.py --suite benchmark/spec/suites/my_suite_v1.json && \
python3 -m benchmark.runner.run_suite --suite benchmark/spec/suites/my_suite_v1.json --generate-manifests && \
python3 scripts/aggregate_results.py && \
python3 -m benchmark.report --suite my-suite-v1 --format md
```
