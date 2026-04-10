# Visualization Plan (RQ-Aligned)

## Data Products Already Generated

- `analysis/metrics_enriched.csv`  
  Run-level metrics with config metadata, completion ratio, per-task overhead rates, scheduler/workload classes.
- `analysis/weighted_allocation_summary.csv`  
  Run-level weighted-allocation proxies (share error, proportional Jain, weight-vs-slowdown/response correlation).
- `analysis/nice_bucket_allocation.csv`  
  Nice-bucket aggregates including observed vs expected share and avg response/wait/slowdown.
- `analysis/workload_characteristics.csv`  
  Per-run workload descriptors (execution/inter-arrival CV, arrival span, weight spread).
- `analysis/fairness_vs_characteristics.csv`  
  Fairness-oriented scheduler deltas vs MLFQ, joined with workload-characteristic features.

## RQ1: Fairness Scheduling vs Throughput

1. Grouped bar chart: `Throughput` by `Scheduler`, faceted by (`Workload`, `Topology`, `Cores`).
2. Companion chart: `ThroughputDeltaVsMLFQ` for fairness-oriented schedulers (CFS/EEVDF/Stride).

Why useful:
- Separates raw throughput from comparative effect vs baseline.
- Prevents topology/workload confounding by faceting.

Source:
- `analysis/metrics_enriched.csv`
- `analysis/fairness_vs_characteristics.csv`

## RQ2: Proportional CPU Allocation Under Weights

1. Heatmap: `WeightVsSlowdownCorr` by (`Scheduler`, `Workload`, `Topology`).
2. Line plot by nice level: `AvgSlowdown` vs `Nice`, one line per scheduler.
3. Calibration chart: `ShareDelta` (observed share minus expected weight share) by nice bucket.

Why useful:
- Correlation shows whether higher-weight tasks actually receive lower slowdown.
- Nice-bucket lines expose policy behavior hidden by single scalar fairness indices.

Source:
- `analysis/weighted_allocation_summary.csv`
- `analysis/nice_bucket_allocation.csv`

## RQ3: Synthetic vs Real Workloads

1. Two-panel box/violin charts for `MeanRT` and `Throughput`, grouped by `Scheduler`, split by `WorkloadType`.
2. Completion-ratio bars (`CompletionRatio`) to contextualize finite-horizon trace runs.

Why useful:
- Directly quantifies domain shift from synthetic to trace workloads.
- Guards against misleading RT/throughput interpretation when completion differs.

Source:
- `analysis/metrics_enriched.csv`

## RQ4: Workload Characteristics Where Fairness Helps/Hurts

1. Scatter: `MeanRTDeltaVsMLFQ` vs `ExecutionCV`, color=`Scheduler`, marker=`WorkloadType`.
2. Scatter: `ThroughputDeltaVsMLFQ` vs `InterArrivalCV`, same encoding.
3. Bubble/scatter: `MeanRTDeltaVsMLFQ` vs `WeightCV`, bubble size=`UniqueNice`.

Why useful:
- Connects scheduler outcomes to measurable workload properties.
- Identifies regimes where fairness is beneficial vs costly.

Source:
- `analysis/fairness_vs_characteristics.csv`

## RQ5: Scheduling Overhead

1. Grouped bars: `ContextSwitchesPerTask` and `PreemptionsPerTask` by scheduler, faceted by workload.
2. Tradeoff scatter: `ContextSwitchesPerTask` (x) vs `MeanRT` (y), color=`Scheduler`.

Why useful:
- Makes overhead explicit and comparable across policies.
- Shows whether extra fairness machinery translates into latency benefit or pure cost.

Source:
- `analysis/metrics_enriched.csv`

## Reproducible Generation Steps

```bash
make -j4
./scripts/run_matrix.sh
./scripts/aggregate_results.py
```

Or full pipeline:

```bash
./scripts/run_full_analysis.sh
```

These commands regenerate `runs/` and all `analysis/*.csv` outputs from scratch.
