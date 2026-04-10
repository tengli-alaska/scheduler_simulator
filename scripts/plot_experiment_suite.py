#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable


pd = None
sns = None
plt = None

SCHEDULER_ORDER = ["CFS", "EEVDF", "MLFQ", "Stride"]
WORKLOAD_ORDER = ["Desktop", "Server", "GoogleTraceV3", "AlibabaTraceV2018"]
EXP5_VARIANT_ORDER = [
    "SQ-4c",
    "MQ-RR-4c-WS-on",
    "MQ-LL-4c-WS-on",
    "MQ-LL-4c-WS-off",
    "MQ-RR-4c-WS-off",
]
PALETTE = {
    "CFS": "#0072B2",
    "EEVDF": "#D55E00",
    "MLFQ": "#009E73",
    "Stride": "#CC79A7",
}


def import_plotting_stack() -> None:
    global pd, sns, plt
    try:
        import pandas as _pd
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
    pd, sns, plt = _pd, _sns, _plt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate figures aligned to Exp1..Exp6 experiment definitions."
    )
    parser.add_argument("--analysis-dir", default="analysis")
    parser.add_argument("--output-dir", default="figures/experiments")
    parser.add_argument("--formats", default="png,pdf")
    parser.add_argument("--dpi", type=int, default=300)
    return parser.parse_args()


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


def finalize_with_top_legend(fig, title: str, handles, labels, ncol: int, top_rect: float = 0.86) -> None:
    fig.suptitle(title, y=0.99)
    fig.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.955),
        ncol=ncol,
        frameon=False,
    )
    fig.tight_layout(rect=(0.0, 0.0, 1.0, top_rect))


def read_metrics(analysis_dir: Path):
    path = analysis_dir / "metrics_enriched.csv"
    if not path.exists():
        raise SystemExit(f"Missing required file: {path}")
    m = pd.read_csv(path)
    required = {"Exp1", "Exp2", "Exp3", "Exp4", "Exp5", "Exp6", "TopologyVariant"}
    missing = [c for c in required if c not in m.columns]
    if missing:
        raise SystemExit(
            "metrics_enriched.csv is missing experiment tags. "
            "Re-run scripts/aggregate_results.py from the latest code.\n"
            f"Missing columns: {missing}"
        )
    for col in [
        "Throughput",
        "MeanRT",
        "JainsFairness",
        "ContextSwitchesPerTask",
        "PreemptionsPerTask",
        "Cores",
        "NumTasks",
    ]:
        if col in m.columns:
            m[col] = pd.to_numeric(m[col], errors="coerce")
    return m


def exp_subset(metrics, exp_col: str):
    sub = metrics[metrics[exp_col] == 1].copy()
    if sub.empty:
        print(f"[warn] No rows matched {exp_col}; skipping figure.")
    return sub


def plot_exp1_multicore_scaling(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp1")
    if df.empty:
        return False
    workloads = ["Desktop", "Server"]
    fig, axes = plt.subplots(1, 2, figsize=(14, 5), sharey=False)
    for i, workload in enumerate(workloads):
        ax = axes[i]
        sub = df[df["Workload"] == workload]
        sns.lineplot(
            data=sub,
            x="Cores",
            y="Throughput",
            hue="Scheduler",
            hue_order=SCHEDULER_ORDER,
            palette=PALETTE,
            marker="o",
            estimator="mean",
            errorbar=("ci", 95),
            ax=ax,
        )
        ax.set_title(workload)
        ax.set_xlabel("Cores")
        ax.set_ylabel("Throughput (tasks/s)")
        if i != 0:
            ax.get_legend().remove()
    handles, labels = axes[0].get_legend_handles_labels()
    finalize_with_top_legend(
        fig,
        "Exp 1: Multi-core Scaling (Throughput vs Cores)",
        handles,
        labels,
        ncol=4,
    )
    save_figure(fig, out_dir, "exp1_multicore_scaling_throughput", formats, dpi)
    return True


def plot_exp2_high_load_convergence(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp2")
    if df.empty:
        return False
    workloads = ["Desktop", "Server"]
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), sharex="col")
    for r, workload in enumerate(workloads):
        sub = df[df["Workload"] == workload]
        ax_t = axes[r, 0]
        ax_rt = axes[r, 1]
        sns.lineplot(
            data=sub,
            x="NumTasks",
            y="Throughput",
            hue="Scheduler",
            hue_order=SCHEDULER_ORDER,
            palette=PALETTE,
            marker="o",
            estimator="mean",
            errorbar=("ci", 95),
            ax=ax_t,
        )
        sns.lineplot(
            data=sub,
            x="NumTasks",
            y="MeanRT",
            hue="Scheduler",
            hue_order=SCHEDULER_ORDER,
            palette=PALETTE,
            marker="o",
            estimator="mean",
            errorbar=("ci", 95),
            ax=ax_rt,
        )
        ax_t.set_title(f"{workload}: Throughput")
        ax_rt.set_title(f"{workload}: Mean Response Time")
        ax_t.set_xscale("log")
        ax_rt.set_xscale("log")
        ax_t.set_xlabel("Num Tasks")
        ax_rt.set_xlabel("Num Tasks")
        ax_t.set_ylabel("Throughput (tasks/s)")
        ax_rt.set_ylabel("Mean Response Time")
        if r != 0:
            ax_t.get_legend().remove()
            ax_rt.get_legend().remove()
    handles, labels = axes[0, 0].get_legend_handles_labels()
    axes[0, 1].get_legend().remove()
    finalize_with_top_legend(
        fig,
        "Exp 2: High-load Convergence (Graceful Degradation)",
        handles,
        labels,
        ncol=4,
    )
    save_figure(fig, out_dir, "exp2_high_load_convergence", formats, dpi)
    return True


def plot_exp3_scheduler_workload_matrix(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp3")
    if df.empty:
        return False
    piv_thr = (
        df.pivot_table(index="Scheduler", columns="Workload", values="Throughput", aggfunc="mean")
        .reindex(index=SCHEDULER_ORDER, columns=WORKLOAD_ORDER)
    )
    piv_rt = (
        df.pivot_table(index="Scheduler", columns="Workload", values="MeanRT", aggfunc="mean")
        .reindex(index=SCHEDULER_ORDER, columns=WORKLOAD_ORDER)
    )
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    sns.heatmap(piv_thr, annot=True, fmt=".2f", cmap="Blues", linewidths=0.5, ax=axes[0])
    sns.heatmap(piv_rt, annot=True, fmt=".1f", cmap="Oranges", linewidths=0.5, ax=axes[1])
    axes[0].set_title("Throughput (tasks/s)")
    axes[1].set_title("Mean Response Time")
    fig.suptitle("Exp 3: Scheduler × Workload Matrix", y=0.99)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.95))
    save_figure(fig, out_dir, "exp3_scheduler_workload_matrix", formats, dpi)
    return True


def plot_exp4_fairness_vs_latency(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp4")
    if df.empty:
        return False
    fig, ax = plt.subplots(figsize=(10, 7))
    sns.scatterplot(
        data=df,
        x="JainsFairness",
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        style="Workload",
        palette=PALETTE,
        s=90,
        alpha=0.8,
        ax=ax,
    )
    ax.set_title("Exp 4: Fairness vs Latency Tradeoff")
    ax.set_xlabel("Jain's Fairness")
    ax.set_ylabel("Mean Response Time")
    ax.set_yscale("log")
    ax.legend(frameon=False, ncol=2)
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
    save_figure(fig, out_dir, "exp4_fairness_vs_latency", formats, dpi)
    return True


def plot_exp5_topology_comparison(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp5")
    if df.empty:
        return False
    fig, axes = plt.subplots(1, 2, figsize=(16, 6), sharey=False)
    sns.barplot(
        data=df,
        x="TopologyVariant",
        y="Throughput",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=EXP5_VARIANT_ORDER,
        palette=PALETTE,
        ci=95,
        ax=axes[0],
    )
    sns.barplot(
        data=df,
        x="TopologyVariant",
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=EXP5_VARIANT_ORDER,
        palette=PALETTE,
        ci=95,
        ax=axes[1],
    )
    axes[0].set_title("Throughput")
    axes[1].set_title("Mean Response Time")
    axes[0].set_ylabel("Throughput (tasks/s)")
    axes[1].set_ylabel("Mean Response Time")
    axes[0].set_xlabel("")
    axes[1].set_xlabel("")
    axes[0].tick_params(axis="x", rotation=25)
    axes[1].tick_params(axis="x", rotation=25)
    handles, labels = axes[0].get_legend_handles_labels()
    axes[0].get_legend().remove()
    axes[1].get_legend().remove()
    finalize_with_top_legend(
        fig,
        "Exp 5: Topology Comparison (Desktop, 4 cores)",
        handles,
        labels,
        ncol=4,
    )
    save_figure(fig, out_dir, "exp5_topology_comparison", formats, dpi)
    return True


def plot_exp6_overhead_cost(metrics, out_dir: Path, formats, dpi: int):
    df = exp_subset(metrics, "Exp6")
    if df.empty:
        return False
    fig, axes = plt.subplots(1, 2, figsize=(16, 6), sharey=False)
    sns.barplot(
        data=df,
        x="Workload",
        y="ContextSwitchesPerTask",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=WORKLOAD_ORDER,
        palette=PALETTE,
        ci=95,
        ax=axes[0],
    )
    sns.barplot(
        data=df,
        x="Workload",
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=WORKLOAD_ORDER,
        palette=PALETTE,
        ci=95,
        ax=axes[1],
    )
    axes[0].set_title("Context Switches / Task")
    axes[1].set_title("Mean Response Time")
    axes[0].set_ylabel("Context Switches per Completed Task")
    axes[1].set_ylabel("Mean Response Time")
    axes[0].set_xlabel("")
    axes[1].set_xlabel("")
    axes[0].set_yscale("log")
    axes[1].set_yscale("log")
    axes[0].tick_params(axis="x", rotation=20)
    axes[1].tick_params(axis="x", rotation=20)
    handles, labels = axes[0].get_legend_handles_labels()
    axes[0].get_legend().remove()
    axes[1].get_legend().remove()
    finalize_with_top_legend(
        fig,
        "Exp 6: Overhead Cost Analysis (from Exp 3 runs)",
        handles,
        labels,
        ncol=4,
    )
    save_figure(fig, out_dir, "exp6_overhead_cost_analysis", formats, dpi)
    return True


def write_index(out_dir: Path, generated: list[str]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    lines = ["# Experiment Figure Index", ""]
    if not generated:
        lines.append("No figures generated (no matching experiment-tagged rows found).")
    else:
        lines.extend(f"- `{name}`" for name in generated)
    lines.append("")
    lines.append("Data source: `analysis/metrics_enriched.csv` with Exp1..Exp6 tags.")
    (out_dir / "figure_index.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    args = parse_args()
    import_plotting_stack()
    setup_style()

    analysis_dir = Path(args.analysis_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    formats = [f.strip() for f in args.formats.split(",") if f.strip()]

    metrics = read_metrics(analysis_dir)

    generated = []
    if plot_exp1_multicore_scaling(metrics, output_dir, formats, args.dpi):
        generated.append("exp1_multicore_scaling_throughput")
    if plot_exp2_high_load_convergence(metrics, output_dir, formats, args.dpi):
        generated.append("exp2_high_load_convergence")
    if plot_exp3_scheduler_workload_matrix(metrics, output_dir, formats, args.dpi):
        generated.append("exp3_scheduler_workload_matrix")
    if plot_exp4_fairness_vs_latency(metrics, output_dir, formats, args.dpi):
        generated.append("exp4_fairness_vs_latency")
    if plot_exp5_topology_comparison(metrics, output_dir, formats, args.dpi):
        generated.append("exp5_topology_comparison")
    if plot_exp6_overhead_cost(metrics, output_dir, formats, args.dpi):
        generated.append("exp6_overhead_cost_analysis")

    write_index(output_dir, generated)
    print(f"Wrote experiment figures to: {output_dir}")


if __name__ == "__main__":
    main()
