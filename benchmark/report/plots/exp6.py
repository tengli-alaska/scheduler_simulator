from __future__ import annotations

from ..layout import (
    PALETTE,
    SCHEDULER_ORDER,
    WORKLOAD_ORDER,
    exp_subset,
    finalize_with_top_legend,
    save_figure,
    set_log_if_possible,
)


EXPERIMENT_ID = "exp6_overhead_cost_analysis"
TITLE = "Exp 6: Overhead Cost Analysis (from Exp 3 runs)"
STEM = "exp6_overhead_cost_analysis"


def plot(metrics, ctx):
    df = exp_subset(metrics, EXPERIMENT_ID)
    if df.empty:
        # Exp 6 reuses Exp 3 runs in canonical suite definitions.
        df = exp_subset(metrics, "exp3_scheduler_workload_matrix")
    if df.empty:
        print(f"[warn] No rows matched {EXPERIMENT_ID}; skipping figure.")
        return None

    fig, axes = ctx.plt.subplots(1, 2, figsize=(16, 6), sharey=False)
    ctx.sns.barplot(
        data=df,
        x="Workload",
        y="ContextSwitchesPerTask",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=WORKLOAD_ORDER,
        palette=PALETTE,
        errorbar=("ci", 95),
        ax=axes[0],
    )
    ctx.sns.barplot(
        data=df,
        x="Workload",
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=WORKLOAD_ORDER,
        palette=PALETTE,
        errorbar=("ci", 95),
        ax=axes[1],
    )
    axes[0].set_title("Context Switches / Task")
    axes[1].set_title("Mean Response Time")
    axes[0].set_ylabel("Context Switches per Completed Task (switches/task)")
    axes[1].set_ylabel("Mean Response Time (ms)")
    axes[0].set_xlabel("Workload (categorical)")
    axes[1].set_xlabel("Workload (categorical)")
    set_log_if_possible(ctx.pd, ctx.np, axes[0], df["ContextSwitchesPerTask"].to_numpy())
    set_log_if_possible(ctx.pd, ctx.np, axes[1], df["MeanRT"].to_numpy())
    axes[0].tick_params(axis="x", rotation=20)
    axes[1].tick_params(axis="x", rotation=20)
    handles, labels = axes[0].get_legend_handles_labels()
    axes[0].get_legend().remove()
    axes[1].get_legend().remove()
    finalize_with_top_legend(
        fig,
        TITLE,
        handles,
        labels,
        ncol=4,
        top_rect=0.90,
    )

    files = save_figure(ctx.plt, fig, ctx.output_dir, STEM, ctx.formats, ctx.dpi)
    return {"experiment_id": EXPERIMENT_ID, "title": TITLE, "stem": STEM, "files": files}
