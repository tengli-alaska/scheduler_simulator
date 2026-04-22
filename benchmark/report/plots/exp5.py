from __future__ import annotations

from ..layout import (
    EXP5_VARIANT_ORDER,
    PALETTE,
    SCHEDULER_ORDER,
    exp_subset,
    finalize_with_top_legend,
    save_figure,
)


EXPERIMENT_ID = "exp5_topology_comparison"
TITLE = "Exp 5: Topology Comparison"
STEM = "exp5_topology_comparison"


def plot(metrics, ctx):
    df = exp_subset(metrics, EXPERIMENT_ID)
    if df.empty:
        print(f"[warn] No rows matched {EXPERIMENT_ID}; skipping figure.")
        return None

    variants_present = set(df["TopologyVariant"].dropna().unique())
    base_order = [v for v in EXP5_VARIANT_ORDER if v in variants_present]
    extras = sorted(variants_present - set(base_order))
    variant_order = base_order + extras

    unique_n = sorted(int(v) for v in df["NumTasks"].dropna().unique())
    df_plot = df.copy()
    x_col = "TopologyVariant"
    x_order = variant_order
    if len(unique_n) > 1:
        df_plot["VariantLabel"] = df_plot.apply(
            lambda r: f"{r['TopologyVariant']} (n={int(r['NumTasks']):,})",
            axis=1,
        )
        x_col = "VariantLabel"
        x_order = []
        for n in unique_n:
            for v in variant_order:
                label = f"{v} (n={n:,})"
                if (df_plot["VariantLabel"] == label).any():
                    x_order.append(label)

    fig, axes = ctx.plt.subplots(1, 2, figsize=(16, 6), sharey=False)
    ctx.sns.barplot(
        data=df_plot,
        x=x_col,
        y="Throughput",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=x_order,
        palette=PALETTE,
        errorbar=("ci", 95),
        ax=axes[0],
    )
    ctx.sns.barplot(
        data=df_plot,
        x=x_col,
        y="MeanRT",
        hue="Scheduler",
        hue_order=SCHEDULER_ORDER,
        order=x_order,
        palette=PALETTE,
        errorbar=("ci", 95),
        ax=axes[1],
    )
    axes[0].set_title("Throughput")
    axes[1].set_title("Mean Response Time")
    axes[0].set_ylabel("Throughput (tasks/s)")
    axes[1].set_ylabel("Mean Response Time (ms)")
    axes[0].set_xlabel("Topology Variant (categorical)")
    axes[1].set_xlabel("Topology Variant (categorical)")
    axes[0].tick_params(axis="x", rotation=25)
    axes[1].tick_params(axis="x", rotation=25)
    handles, labels = axes[0].get_legend_handles_labels()
    axes[0].get_legend().remove()
    axes[1].get_legend().remove()
    n_label = ", ".join(f"{n:,}" for n in unique_n)
    subtitle = f"Workload: GoogleTraceV3  |  Cores: 4  |  Stop Time: 500,000  |  Tasks: {n_label}"
    finalize_with_top_legend(
        fig,
        TITLE,
        handles,
        labels,
        ncol=4,
        subtitle=subtitle,
        top_rect=0.89,
    )

    files = save_figure(ctx.plt, fig, ctx.output_dir, STEM, ctx.formats, ctx.dpi)
    return {"experiment_id": EXPERIMENT_ID, "title": TITLE, "stem": STEM, "files": files}
