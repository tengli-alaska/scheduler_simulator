from __future__ import annotations

import argparse
from pathlib import Path

from .data import (
    filter_frames_by_suite,
    import_plotting_stack,
    load_analysis_frames,
    resolve_suite_id,
)
from .layout import setup_style
from .markdown import build_markdown_report
from .pipeline import generate_experiment_figures, write_figure_index


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate modular experiment figures and a reproducible benchmark report."
        )
    )
    parser.add_argument(
        "--suite",
        default="full",
        help="Suite selector: full, suite_id, or path to suite JSON spec.",
    )
    parser.add_argument(
        "--format",
        default="md",
        choices=["md"],
        help="Report output format.",
    )
    parser.add_argument("--analysis-dir", default="analysis")
    parser.add_argument("--figures-dir", default="figures/publication")
    parser.add_argument("--report-dir", default="reports")
    parser.add_argument("--figure-formats", default="png,pdf")
    parser.add_argument("--dpi", type=int, default=300)
    return parser.parse_args()


def _resolve_path(root_dir: Path, value: str) -> Path:
    p = Path(value)
    if p.is_absolute():
        return p
    return (root_dir / p).resolve()


def main() -> None:
    args = parse_args()
    root_dir = Path(__file__).resolve().parents[2]

    suite_id = resolve_suite_id(args.suite)
    analysis_dir = _resolve_path(root_dir, args.analysis_dir)
    figures_base = _resolve_path(root_dir, args.figures_dir)
    report_base = _resolve_path(root_dir, args.report_dir)

    pd, sns, plt, np = import_plotting_stack()
    setup_style(sns, plt)

    frames = load_analysis_frames(analysis_dir, pd)
    frames = filter_frames_by_suite(frames, suite_id)
    metrics = frames["metrics"]
    if metrics.empty:
        raise SystemExit(
            f"No metrics rows available after suite filter '{suite_id}'. "
            f"Check SuiteId values in {analysis_dir / 'metrics_enriched.csv'}."
        )

    suite_slug = suite_id if suite_id else "full"
    fig_output_dir = (figures_base / suite_slug).resolve()
    formats = [f.strip() for f in args.figure_formats.split(",") if f.strip()]

    generated = generate_experiment_figures(
        metrics=metrics,
        output_dir=fig_output_dir,
        formats=formats,
        dpi=args.dpi,
        pd=pd,
        sns=sns,
        plt=plt,
        np=np,
    )
    index_path = write_figure_index(fig_output_dir, generated)

    if args.format == "md":
        report_path = (report_base / suite_slug / "report.md").resolve()
        build_markdown_report(
            root_dir=root_dir,
            suite_id=suite_id,
            report_path=report_path,
            generated_figures=generated,
            frames=frames,
            pd=pd,
            sns=sns,
            plt=plt,
        )
        print(f"Wrote report: {report_path}")

    print(f"Wrote publication figures to: {fig_output_dir}")
    print(f"Wrote figure index: {index_path}")


if __name__ == "__main__":
    main()
