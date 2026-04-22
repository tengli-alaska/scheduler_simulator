from __future__ import annotations

from ..layout import (
    PALETTE,
    SCHEDULER_ORDER,
    exp_subset,
    finalize_with_top_legend,
    ordered_workloads_from_df,
    save_figure,
    set_log_if_possible,
)


EXPERIMENT_ID = "exp2_high_load_convergence"
TITLE = "Exp 2: High-load Convergence (Graceful Degradation)"
STEM = "exp2_high_load_convergence"


def plot(metrics, ctx):
    df = exp_subset(metrics, EXPERIMENT_ID)
    if df.empty:
        print(f"[warn] No rows matched {EXPERIMENT_ID}; skipping figure.")
        return None

    workloads = ordered_workloads_from_df(df)
    if not workloads:
        print(f"[warn] {EXPERIMENT_ID} has no workloads after filtering; skipping figure.")
        return None

    panels = [
        ("MeanRT", "Mean Response Time (ms)", True),
        ("P99RT", "P99 Response Time (ms)", True),
        ("JainsFairness", "Jain's Fairness Index (0-1)", False),
    ]

    agg = (
        df[df["Workload"].isin(workloads)]
        .groupby(["Workload", "Scheduler", "NumTasks"])[[m for m, _, _ in panels]]
        .mean()
        .reset_index()
    )

    n_vals = sorted(agg["NumTasks"].unique())
    n_order = [str(int(n)) for n in n_vals]
    agg["NumTasksStr"] = agg["NumTasks"].apply(lambda v: str(int(v)))

    x = ctx.np.arange(len(n_order))
    bar_width = 0.18

    fig, axes = ctx.plt.subplots(
        len(panels),
        len(workloads),
        figsize=(13, 3.8 * len(panels)),
        sharey=False,
        squeeze=False,
    )

    for col, workload in enumerate(workloads):
        sub = agg[agg["Workload"] == workload]
        for row, (metric, ylabel, use_log) in enumerate(panels):
            ax = axes[row, col]
            for i, sched in enumerate(SCHEDULER_ORDER):
                sdata = sub[sub["Scheduler"] == sched].set_index("NumTasksStr").reindex(n_order)
                vals = sdata[metric].values
                offset = (i - (len(SCHEDULER_ORDER) - 1) / 2) * bar_width
                ax.bar(
                    x + offset,
                    vals,
                    width=bar_width * 0.9,
                    label=sched,
                    color=PALETTE[sched],
                    edgecolor="white",
                    linewidth=0.5,
                )
            ax.set_xticks(x)
            ax.set_xticklabels([f"{int(n):,}" for n in n_vals], rotation=15, ha="right")
            ax.set_xlabel("Number of Tasks (count)")
            ax.set_ylabel(ylabel)
            if use_log:
                set_log_if_possible(ctx.pd, ctx.np, ax, sub[metric].to_numpy())
            if row == 0:
                ax.set_title(workload, fontweight="bold", pad=10)
            ax.yaxis.grid(True, alpha=0.4, zorder=0)
            ax.set_axisbelow(True)

    handles = [ctx.plt.Rectangle((0, 0), 1, 1, color=PALETTE[s], label=s) for s in SCHEDULER_ORDER]
    labels = SCHEDULER_ORDER
    n_str = ", ".join(f"{int(n):,}" for n in n_vals)
    stop_times = sorted(df[df["Workload"].isin(workloads)]["StopTime"].unique())
    t_str = ", ".join(f"{int(t):,}" for t in stop_times)
    subtitle = (
        f"Tasks varied: {n_str}  |  Stop Time(s): {t_str}  |  "
        f"Cores: 4  |  Queue: Single  |  Workloads: {', '.join(workloads)}"
    )
    finalize_with_top_legend(
        fig,
        TITLE,
        handles,
        labels,
        ncol=4,
        top_rect=0.90,
        subtitle=subtitle,
    )

    files = save_figure(ctx.plt, fig, ctx.output_dir, STEM, ctx.formats, ctx.dpi)
    return {"experiment_id": EXPERIMENT_ID, "title": TITLE, "stem": STEM, "files": files}
