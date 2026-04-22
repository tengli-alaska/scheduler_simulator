from __future__ import annotations

from ..layout import PALETTE, SCHEDULER_ORDER, exp_subset, safe_tight_layout, save_figure, set_log_if_possible


EXPERIMENT_ID = "exp4_fairness_vs_latency"
TITLE = "Exp 4: Fairness vs Latency Tradeoff"
STEM = "exp4_fairness_vs_latency"


def plot(metrics, ctx):
    df = exp_subset(metrics, EXPERIMENT_ID)
    if df.empty:
        # Exp 4 reuses Exp 3 runs in canonical suite definitions.
        df = exp_subset(metrics, "exp3_scheduler_workload_matrix")
    if df.empty:
        print(f"[warn] No rows matched {EXPERIMENT_ID}; skipping figure.")
        return None

    fig, ax = ctx.plt.subplots(figsize=(10, 7))
    ctx.sns.scatterplot(
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
    ax.set_title(TITLE)
    ax.set_xlabel("Jain's Fairness Index (0-1)")
    ax.set_ylabel("Mean Response Time (ms)")
    set_log_if_possible(ctx.pd, ctx.np, ax, df["MeanRT"].to_numpy())
    ax.legend(frameon=False, ncol=2)
    safe_tight_layout(fig, rect=(0.0, 0.0, 1.0, 0.97))

    files = save_figure(ctx.plt, fig, ctx.output_dir, STEM, ctx.formats, ctx.dpi)
    return {"experiment_id": EXPERIMENT_ID, "title": TITLE, "stem": STEM, "files": files}
