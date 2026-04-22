from __future__ import annotations

from pathlib import Path

from .layout import PlotContext
from .plots import PLOT_MODULES


def generate_experiment_figures(
    metrics,
    output_dir: Path,
    formats: list[str],
    dpi: int,
    pd,
    sns,
    plt,
    np,
) -> list[dict]:
    ctx = PlotContext(
        pd=pd,
        sns=sns,
        plt=plt,
        np=np,
        output_dir=output_dir,
        formats=formats,
        dpi=dpi,
    )

    generated: list[dict] = []
    for module in PLOT_MODULES:
        result = module.plot(metrics, ctx)
        if result is not None:
            generated.append(result)
    return generated


def write_figure_index(output_dir: Path, generated: list[dict]) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    lines = ["# Publication Figure Index", ""]
    if not generated:
        lines.append("No figures generated (no matching experiment rows found).")
    else:
        for item in generated:
            line = f"- `{item['stem']}` ({item['experiment_id']})"
            lines.append(line)
    lines.append("")
    lines.append("Data source: `analysis/metrics_enriched.csv`.")
    index_path = output_dir / "figure_index.md"
    index_path.write_text("\n".join(lines), encoding="utf-8")
    return index_path
