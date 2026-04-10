#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable


pd = None
np = None
sns = None
plt = None


SCHEDULER_ORDER = ["CFS", "EEVDF", "MLFQ", "Stride"]
FAIR_SCHEDULER_ORDER = ["CFS", "EEVDF", "Stride"]
WORKLOAD_ORDER = ["Server", "Desktop", "AlibabaTraceV2018", "GoogleTraceV3"]
SCENARIO_ORDER = ["SQSS-1c", "SQMS-4c", "MQMS-4c-WS-on", "MQMS-4c-WS-off"]
PALETTE = {
    "CFS": "#0072B2",
    "EEVDF": "#D55E00",
    "MLFQ": "#009E73",
    "Stride": "#CC79A7",
}


def import_plotting_stack() -> None:
    global pd, np, sns, plt
    try:
        import pandas as _pd
        import numpy as _np
        import seaborn as _sns
        import matplotlib.pyplot as _plt
    except ImportError as exc:
        msg = (
            "Missing plotting dependencies.\n"
            "Install with:\n"
            "  python3 -m pip install -r scripts/plot_requirements.txt\n"
            f"Original error: {exc}"
        )
        raise SystemExit(msg) from exc
    pd, np, sns, plt = _pd, _np, _sns, _plt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate publication figures from analysis/*.csv outputs."
    )
    parser.add_argument(
        "--analysis-dir",
        default="analysis",
        help="Directory containing analysis CSV files (default: analysis)",
    )
    parser.add_argument(
        "--output-dir",
        default="figures",
        help="Directory to write figures (default: figures)",
    )
    parser.add_argument(
        "--formats",
        default="png,pdf",
        help="Comma-separated output formats (default: png,pdf)",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=300,
        help="Raster output DPI for png format (default: 300)",
    )
    return parser.parse_args()


def scenario_label(row) -> str:
    topology = str(row["Topology"]).strip().lower()
    cores = int(row["Cores"])
    ws = str(row.get("WorkStealing", "na")).strip().lower()

    if topology == "sq" and cores == 1:
        return "SQSS-1c"
    if topology == "sq" and cores == 4:
        return "SQMS-4c"
    if topology == "mq" and cores == 4 and ws == "on":
        return "MQMS-4c-WS-on"
    if topology == "mq" and cores == 4 and ws == "off":
        return "MQMS-4c-WS-off"
    return f"{topology.upper()}-{cores}c-{ws}"


def setup_style() -> None:
    sns.set_theme(style="whitegrid", context="paper")
    plt.rcParams.update(
        {
            "figure.dpi": 120,
            "savefig.dpi": 300,
            "font.family": "DejaVu Sans",
            "axes.titlesize": 11,
            "axes.labelsize": 10,
            "legend.fontsize": 9,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
        }
    )


def read_csv(analysis_dir: Path, name: str):
    path = analysis_dir / name
    if not path.exists():
        raise SystemExit(f"Required file missing: {path}")
    return pd.read_csv(path)


def save_figure(fig, output_dir: Path, stem: str, formats: Iterable[str], dpi: int) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for fmt in formats:
        fmt = fmt.strip().lower()
        if not fmt:
            continue
        target = output_dir / f"{stem}.{fmt}"
        if fmt in {"png", "jpg", "jpeg", "tif", "tiff"}:
            fig.savefig(target, bbox_inches="tight", dpi=dpi)
        else:
            fig.savefig(target, bbox_inches="tight")
    plt.close(fig)


def finalize_with_top_legend(
    fig,
    title: str,
    handles,
    labels,
    *,
    legend_ncol: int,
    top_rect: float = 0.86,
) -> None:
    # Reserve a stable header band so figure-level title and legend never overlap.
    fig.suptitle(title, y=0.99)
    fig.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.955),
        ncol=legend_ncol,
        frameon=False,
    )
    fig.tight_layout(rect=(0.0, 0.0, 1.0, top_rect))


def add_scenario_column(df):
    df = df.copy()
    df["Scenario"] = df.apply(scenario_label, axis=1)
    return df


def plot_rq1_throughput(metrics, fairness_chars, output_dir: Path, formats, dpi: int) -> None:
    m = add_scenario_column(metrics)

    fig, axes = plt.subplots(2, 2, figsize=(16, 10), sharey=False)
    axes = axes.flatten()
    for i, workload in enumerate(WORKLOAD_ORDER):
        ax = axes[i]
        sub = m[m["Workload"] == workload]
        sns.barplot(
            data=sub,
            x="Scenario",
            y="Throughput",
            hue="Scheduler",
            order=SCENARIO_ORDER,
            hue_order=SCHEDULER_ORDER,
            palette=PALETTE,
            ci=95,
            ax=ax,
        )
        ax.set_title(workload)
        ax.set_xlabel("")
        ax.set_ylabel("Throughput (tasks/s)")
        ax.tick_params(axis="x", rotation=20)
        if i != 0 and ax.get_legend() is not None:
            ax.get_legend().remove()
    handles, labels = axes[0].get_legend_handles_labels()
    finalize_with_top_legend(
        fig,
        "RQ1: Throughput by Scheduler Across Workloads and Scenarios",
        handles,
        labels,
        legend_ncol=4,
    )
    save_figure(fig, output_dir, "rq1_throughput_by_scheduler", formats, dpi)

    f = add_scenario_column(fairness_chars)
    fig, axes = plt.subplots(2, 2, figsize=(16, 10), sharey=False)
    axes = axes.flatten()
    for i, workload in enumerate(WORKLOAD_ORDER):
        ax = axes[i]
        sub = f[f["Workload"] == workload]
        sns.barplot(
            data=sub,
            x="Scenario",
            y="ThroughputDeltaVsMLFQ",
            hue="Scheduler",
            order=SCENARIO_ORDER,
            hue_order=FAIR_SCHEDULER_ORDER,
            palette=PALETTE,
            ci=95,
            ax=ax,
        )
        ax.axhline(0.0, color="black", linewidth=1, alpha=0.7)
        ax.set_title(workload)
        ax.set_xlabel("")
        ax.set_ylabel("Throughput Delta vs MLFQ (tasks/s)")
        ax.tick_params(axis="x", rotation=20)
        if i != 0 and ax.get_legend() is not None:
            ax.get_legend().remove()
    handles, labels = axes[0].get_legend_handles_labels()
    finalize_with_top_legend(
        fig,
        "RQ1: Throughput Gain/Loss vs MLFQ",
        handles,
        labels,
        legend_ncol=3,
    )
    save_figure(fig, output_dir, "rq1_throughput_delta_vs_mlfq", formats, dpi)


def plot_rq2_weighted_behavior(weighted_alloc, nice_bucket, output_dir: Path, formats, dpi: int) -> None:
    w = add_scenario_column(weighted_alloc)
    fig, axes = plt.subplots(2, 2, figsize=(16, 10))
    axes = axes.flatten()
    for i, scenario in enumerate(SCENARIO_ORDER):
        ax = axes[i]
        sub = w[w["Scenario"] == scenario]
        pivot = (
            sub.pivot_table(
                index="Scheduler",
                columns="Workload",
                values="WeightVsSlowdownCorr",
                aggfunc="mean",
            )
            .reindex(index=SCHEDULER_ORDER, columns=WORKLOAD_ORDER)
        )
        sns.heatmap(
            pivot,
            cmap="coolwarm",
            center=0.0,
            annot=True,
            fmt=".2f",
            linewidths=0.5,
            cbar=(i == 0),
            ax=ax,
        )
        ax.set_title(scenario)
        ax.set_xlabel("")
        ax.set_ylabel("")
    fig.suptitle("RQ2: Weight-vs-Slowdown Correlation (lower is better)", y=0.99)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.95))
    save_figure(fig, output_dir, "rq2_weight_slowdown_correlation_heatmap", formats, dpi)

    n = nice_bucket.groupby(["Scheduler", "Nice"], as_index=False).agg(
        AvgSlowdown=("AvgSlowdown", "mean"),
        ShareDelta=("ShareDelta", "mean"),
    )

    fig, ax = plt.subplots(figsize=(11, 6))
    sns.lineplot(
        data=n,
        x="Nice",
        y="AvgSlowdown",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        palette=PALETTE,
        marker="o",
        ax=ax,
    )
    ax.set_title("RQ2: Nice-Bucket Average Slowdown")
    ax.set_xlabel("Nice Value")
    ax.set_ylabel("Average Slowdown (Turnaround / Execution)")
    ax.legend(title="Scheduler", frameon=False)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
    save_figure(fig, output_dir, "rq2_nice_bucket_slowdown", formats, dpi)

    fig, ax = plt.subplots(figsize=(11, 6))
    sns.lineplot(
        data=n,
        x="Nice",
        y="ShareDelta",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        palette=PALETTE,
        marker="o",
        ax=ax,
    )
    ax.axhline(0.0, color="black", linewidth=1, alpha=0.7)
    ax.set_title("RQ2: CPU Share Calibration by Nice Bucket")
    ax.set_xlabel("Nice Value")
    ax.set_ylabel("Observed Share - Expected Weight Share")
    ax.legend(title="Scheduler", frameon=False)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
    save_figure(fig, output_dir, "rq2_nice_bucket_share_delta", formats, dpi)


def plot_rq3_synth_vs_real(metrics, output_dir: Path, formats, dpi: int) -> None:
    m = metrics.copy()
    m["WorkloadTypeLabel"] = m["WorkloadType"].map(
        {"synthetic": "Synthetic", "real_trace": "Real Trace"}
    )

    fig, axes = plt.subplots(1, 2, figsize=(15, 5))
    sns.boxplot(
        data=m,
        x="Scheduler",
        y="Throughput",
        hue="WorkloadTypeLabel",
        order=SCHEDULER_ORDER,
        ax=axes[0],
    )
    axes[0].set_title("Throughput Distribution")
    axes[0].set_xlabel("")
    axes[0].set_ylabel("Throughput (tasks/s)")

    sns.boxplot(
        data=m,
        x="Scheduler",
        y="MeanRT",
        hue="WorkloadTypeLabel",
        order=SCHEDULER_ORDER,
        ax=axes[1],
    )
    axes[1].set_title("Mean Response Time Distribution")
    axes[1].set_xlabel("")
    axes[1].set_ylabel("Mean Response Time")
    axes[1].set_yscale("log")

    for ax in axes:
        ax.tick_params(axis="x", rotation=20)
    handles, labels = axes[0].get_legend_handles_labels()
    axes[0].legend_.remove()
    axes[1].legend_.remove()
    finalize_with_top_legend(
        fig,
        "RQ3: Synthetic vs Real-Trace Performance",
        handles,
        labels,
        legend_ncol=2,
    )
    save_figure(fig, output_dir, "rq3_synthetic_vs_real", formats, dpi)

    comp = (
        m.groupby(["Workload", "Scheduler"], as_index=False)["CompletionRatio"]
        .mean()
        .sort_values(["Workload", "Scheduler"])
    )
    fig, ax = plt.subplots(figsize=(12, 6))
    sns.barplot(
        data=comp,
        x="Workload",
        y="CompletionRatio",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        palette=PALETTE,
        ax=ax,
    )
    ax.set_ylim(0.0, 1.05)
    ax.set_title("RQ3: Completion Ratio by Workload and Scheduler")
    ax.set_xlabel("")
    ax.set_ylabel("Completion Ratio")
    ax.legend(title="Scheduler", frameon=False)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
    save_figure(fig, output_dir, "rq3_completion_ratio", formats, dpi)


def plot_rq4_characteristics(fairness_chars, output_dir: Path, formats, dpi: int) -> None:
    f = fairness_chars.copy()
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()

    scatter_specs = [
        ("ExecutionCV", "MeanRTDeltaVsMLFQ", "Execution CV", "Mean RT Delta vs MLFQ"),
        ("InterArrivalCV", "MeanRTDeltaVsMLFQ", "Inter-arrival CV", "Mean RT Delta vs MLFQ"),
        ("WeightCV", "MeanRTDeltaVsMLFQ", "Weight CV", "Mean RT Delta vs MLFQ"),
        ("ExecutionCV", "ThroughputDeltaVsMLFQ", "Execution CV", "Throughput Delta vs MLFQ"),
    ]

    for i, (x_col, y_col, x_label, y_label) in enumerate(scatter_specs):
        ax = axes[i]
        sns.scatterplot(
            data=f,
            x=x_col,
            y=y_col,
            hue="Scheduler",
            hue_order=FAIR_SCHEDULER_ORDER,
            style="WorkloadType",
            palette=PALETTE,
            alpha=0.8,
            s=60,
            ax=ax,
        )
        ax.axhline(0.0, color="black", linewidth=1, alpha=0.7)
        ax.set_xlabel(x_label)
        ax.set_ylabel(y_label)
        if i != 0:
            ax.get_legend().remove()
    handles, labels = axes[0].get_legend_handles_labels()
    finalize_with_top_legend(
        fig,
        "RQ4: When Fairness Helps or Hurts",
        handles,
        labels,
        legend_ncol=4,
    )
    save_figure(fig, output_dir, "rq4_workload_characteristics_scatter", formats, dpi)


def plot_rq5_overhead(metrics, output_dir: Path, formats, dpi: int) -> None:
    m = metrics.copy()
    m["WorkloadTypeLabel"] = m["WorkloadType"].map(
        {"synthetic": "Synthetic", "real_trace": "Real Trace"}
    )

    melted = m.melt(
        id_vars=["Scheduler", "WorkloadTypeLabel"],
        value_vars=["ContextSwitchesPerTask", "PreemptionsPerTask"],
        var_name="Metric",
        value_name="Value",
    )
    metric_label = {
        "ContextSwitchesPerTask": "Context Switches / Completed Task",
        "PreemptionsPerTask": "Preemptions / Completed Task",
    }
    melted["MetricLabel"] = melted["Metric"].map(metric_label)

    fig, axes = plt.subplots(1, 2, figsize=(16, 5))
    for i, metric in enumerate(
        ["ContextSwitchesPerTask", "PreemptionsPerTask"]
    ):
        ax = axes[i]
        sub = melted[melted["Metric"] == metric]
        sns.barplot(
            data=sub,
            x="Scheduler",
            y="Value",
            hue="WorkloadTypeLabel",
            order=SCHEDULER_ORDER,
            ci=95,
            ax=ax,
        )
        ax.set_title(metric_label[metric])
        ax.set_xlabel("")
        ax.set_ylabel("Count per Completed Task")
        ax.set_yscale("log")
        ax.tick_params(axis="x", rotation=20)
        if i != 0:
            ax.get_legend().remove()
    handles, labels = axes[0].get_legend_handles_labels()
    finalize_with_top_legend(
        fig,
        "RQ5: Scheduling Overhead",
        handles,
        labels,
        legend_ncol=2,
    )
    save_figure(fig, output_dir, "rq5_overhead_bars", formats, dpi)

    fig, ax = plt.subplots(figsize=(10, 7))
    sns.scatterplot(
        data=m,
        x="ContextSwitchesPerTask",
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        style="Workload",
        palette=PALETTE,
        alpha=0.75,
        s=70,
        ax=ax,
    )
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_title("RQ5: Overhead vs Latency Tradeoff")
    ax.set_xlabel("Context Switches per Completed Task (log scale)")
    ax.set_ylabel("Mean Response Time (log scale)")
    ax.legend(frameon=False, ncol=2)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
    save_figure(fig, output_dir, "rq5_overhead_vs_latency_tradeoff", formats, dpi)


def write_index(output_dir: Path) -> None:
    index_path = output_dir / "figure_index.md"
    lines = [
        "# Figure Index",
        "",
        "- `rq1_throughput_by_scheduler`",
        "- `rq1_throughput_delta_vs_mlfq`",
        "- `rq2_weight_slowdown_correlation_heatmap`",
        "- `rq2_nice_bucket_slowdown`",
        "- `rq2_nice_bucket_share_delta`",
        "- `rq3_synthetic_vs_real`",
        "- `rq3_completion_ratio`",
        "- `rq4_workload_characteristics_scatter`",
        "- `rq5_overhead_bars`",
        "- `rq5_overhead_vs_latency_tradeoff`",
        "",
        "Each figure is emitted in every requested format (for example, png and pdf).",
    ]
    index_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    args = parse_args()
    import_plotting_stack()
    setup_style()

    analysis_dir = Path(args.analysis_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    formats = [f.strip() for f in args.formats.split(",") if f.strip()]

    metrics = read_csv(analysis_dir, "metrics_enriched.csv")
    weighted_alloc = read_csv(analysis_dir, "weighted_allocation_summary.csv")
    nice_bucket = read_csv(analysis_dir, "nice_bucket_allocation.csv")
    fairness_chars = read_csv(analysis_dir, "fairness_vs_characteristics.csv")

    plot_rq1_throughput(metrics, fairness_chars, output_dir, formats, args.dpi)
    plot_rq2_weighted_behavior(weighted_alloc, nice_bucket, output_dir, formats, args.dpi)
    plot_rq3_synth_vs_real(metrics, output_dir, formats, args.dpi)
    plot_rq4_characteristics(fairness_chars, output_dir, formats, args.dpi)
    plot_rq5_overhead(metrics, output_dir, formats, args.dpi)
    write_index(output_dir)

    print(f"Wrote publication figures to: {output_dir}")


if __name__ == "__main__":
    main()
