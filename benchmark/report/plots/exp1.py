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


EXPERIMENT_ID = "exp1_multicore_scaling"
TITLE = "Exp 1: Multi-core Scaling"
STEM = "exp1_multicore_scaling"


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
        ("ContextSwitches", "Context Switches (count)", False),
    ]

    agg = (
        df.groupby(["Workload", "Scheduler", "Cores"])[[m for m, _, _ in panels]]
        .mean()
        .reset_index()
    )
    agg["Cores"] = agg["Cores"].astype(str)
    core_order = sorted(agg["Cores"].unique(), key=lambda v: int(v))
    bar_width = 0.18
    x = ctx.np.arange(len(core_order))

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
                sdata = sub[sub["Scheduler"] == sched].set_index("Cores").reindex(core_order)
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
            ax.set_xticklabels([f"{c}c" for c in core_order])
            ax.set_xlabel("Number of Cores (count)")
            ax.set_ylabel(ylabel)
            if use_log:
                set_log_if_possible(ctx.pd, ctx.np, ax, sub[metric].to_numpy())
            if row == 0:
                ax.set_title(workload, fontweight="bold", pad=10)
            ax.yaxis.grid(True, alpha=0.4, zorder=0)
            ax.set_axisbelow(True)

    handles = [ctx.plt.Rectangle((0, 0), 1, 1, color=PALETTE[s], label=s) for s in SCHEDULER_ORDER]
    labels = SCHEDULER_ORDER
    params = df[["NumTasks", "StopTime"]].iloc[0]
    subtitle = (
        f"Tasks: {int(params['NumTasks']):,}  |  "
        f"Stop Time: {int(params['StopTime']):,}  |  "
        f"Cores varied: {', '.join(core_order)}  |  "
        f"Queue: Single  |  "
        f"Workloads: {', '.join(workloads)}"
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
