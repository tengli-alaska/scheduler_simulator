from __future__ import annotations

from ..layout import SCHEDULER_ORDER, WORKLOAD_ORDER, exp_subset, safe_tight_layout, save_figure


EXPERIMENT_ID = "exp3_scheduler_workload_matrix"
TITLE = "Exp 3: Scheduler x Workload Matrix"
STEM = "exp3_scheduler_workload_matrix"


def plot(metrics, ctx):
    df = exp_subset(metrics, EXPERIMENT_ID)
    if df.empty:
        print(f"[warn] No rows matched {EXPERIMENT_ID}; skipping figure.")
        return None

    piv_thr = (
        df.pivot_table(index="Scheduler", columns="Workload", values="Throughput", aggfunc="mean")
        .reindex(index=SCHEDULER_ORDER, columns=WORKLOAD_ORDER)
    )
    piv_rt = (
        df.pivot_table(index="Scheduler", columns="Workload", values="MeanRT", aggfunc="mean")
        .reindex(index=SCHEDULER_ORDER, columns=WORKLOAD_ORDER)
    )

    fig, axes = ctx.plt.subplots(1, 2, figsize=(16, 6))
    ctx.sns.heatmap(piv_thr, annot=True, fmt=".2f", cmap="Blues", linewidths=0.5, ax=axes[0])
    ctx.sns.heatmap(piv_rt, annot=True, fmt=".1f", cmap="Oranges", linewidths=0.5, ax=axes[1])
    axes[0].set_title("Throughput (tasks/s)")
    axes[1].set_title("Mean Response Time (ms)")
    axes[0].set_xlabel("Workload")
    axes[1].set_xlabel("Workload")
    axes[0].set_ylabel("Scheduler")
    axes[1].set_ylabel("Scheduler")
    fig.suptitle(TITLE, y=0.995)
    safe_tight_layout(fig, rect=(0.0, 0.0, 1.0, 0.94))

    files = save_figure(ctx.plt, fig, ctx.output_dir, STEM, ctx.formats, ctx.dpi)
    return {"experiment_id": EXPERIMENT_ID, "title": TITLE, "stem": STEM, "files": files}
