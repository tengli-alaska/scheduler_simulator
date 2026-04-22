from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


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
EXP_NUM_RE = re.compile(r"^exp(\d+)")


@dataclass
class PlotContext:
    pd: object
    sns: object
    plt: object
    np: object
    output_dir: Path
    formats: list[str]
    dpi: int


def setup_style(sns, plt) -> None:
    sns.set_theme(style="whitegrid", context="paper")
    plt.rcParams.update(
        {
            "figure.dpi": 120,
            "savefig.dpi": 300,
            "font.family": "DejaVu Sans",
            "axes.titlesize": 13,
            "axes.labelsize": 11,
            "legend.fontsize": 10,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.grid": True,
            "grid.alpha": 0.4,
            "lines.linewidth": 2.0,
            "lines.markersize": 7,
        }
    )


def ordered_workloads_from_df(df) -> list[str]:
    present = set(df["Workload"].dropna().unique())
    ordered = [w for w in WORKLOAD_ORDER if w in present]
    extras = sorted(present - set(ordered))
    return ordered + extras


def set_log_if_possible(pd, np, ax, values) -> bool:
    arr = pd.to_numeric(pd.Series(values), errors="coerce").to_numpy(dtype=float)
    pos = arr[np.isfinite(arr) & (arr > 0.0)]
    if pos.size == 0:
        return False
    ax.set_yscale("log")
    ymin = max(float(pos.min()) * 0.8, 1e-12)
    _, ymax = ax.get_ylim()
    if not np.isfinite(ymax) or ymax <= ymin:
        ymax = max(float(pos.max()) * 1.2, ymin * 10.0)
    ax.set_ylim(ymin, ymax)
    return True


def safe_tight_layout(fig, rect) -> None:
    try:
        fig.tight_layout(rect=rect)
    except ValueError as exc:
        if "log-scaled" in str(exc):
            for ax in fig.axes:
                if ax.get_yscale() == "log":
                    ax.set_yscale("linear")
            fig.tight_layout(rect=rect)
        else:
            raise


def finalize_with_top_legend(
    fig,
    title: str,
    handles,
    labels,
    ncol: int,
    top_rect: float = 0.86,
    subtitle: str | None = None,
) -> None:
    # Reserve explicit vertical bands to prevent any title/legend overlap.
    fig.suptitle(title, y=0.995, fontsize=13, fontweight="bold")
    if subtitle:
        fig.text(
            0.5,
            0.970,
            subtitle,
            ha="center",
            va="top",
            fontsize=9,
            color="#555555",
            style="italic",
        )
        legend_y = 0.947
    else:
        legend_y = 0.962
    fig.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, legend_y),
        ncol=ncol,
        frameon=False,
    )
    safe_tight_layout(fig, rect=(0.0, 0.0, 1.0, top_rect))


def save_figure(plt, fig, output_dir: Path, stem: str, formats: Iterable[str], dpi: int) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []
    for fmt in formats:
        fmt = fmt.strip().lower()
        if not fmt:
            continue
        target = output_dir / f"{stem}.{fmt}"
        if fmt in {"png", "jpg", "jpeg", "tif", "tiff"}:
            fig.savefig(target, bbox_inches="tight", dpi=dpi)
        else:
            fig.savefig(target, bbox_inches="tight")
        outputs.append(target)
    plt.close(fig)
    return outputs


def exp_subset(metrics, exp_id: str):
    if "ExperimentIds" not in metrics.columns:
        return metrics.iloc[0:0].copy()

    mask = (
        metrics["ExperimentIds"]
        .fillna("")
        .astype(str)
        .apply(lambda s: exp_id in {p.strip() for p in s.split(";") if p.strip() and p.strip() != "none"})
    )
    if mask.any():
        return metrics[mask].copy()

    m = EXP_NUM_RE.match(exp_id)
    if m:
        col = f"Exp{int(m.group(1))}"
        if col in metrics.columns:
            return metrics[metrics[col] == 1].copy()

    return metrics.iloc[0:0].copy()
