#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from benchmark.report.data import (
    filter_frames_by_suite,
    import_plotting_stack,
    load_analysis_frames,
    resolve_suite_id,
)
from benchmark.report.layout import setup_style
from benchmark.report.pipeline import generate_experiment_figures, write_figure_index


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate Exp1..Exp6 figures using modular benchmark.report plot modules."
    )
    parser.add_argument("--analysis-dir", default="analysis")
    parser.add_argument("--output-dir", default="figures/experiments")
    parser.add_argument("--formats", default="png,pdf")
    parser.add_argument("--dpi", type=int, default=300)
    parser.add_argument(
        "--suite",
        default="full",
        help="Suite selector: full, suite_id, or path to suite JSON spec.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    pd, sns, plt, np = import_plotting_stack()
    setup_style(sns, plt)

    suite_id = resolve_suite_id(args.suite)
    analysis_dir = Path(args.analysis_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    formats = [f.strip() for f in args.formats.split(",") if f.strip()]

    frames = load_analysis_frames(analysis_dir, pd)
    frames = filter_frames_by_suite(frames, suite_id)
    metrics = frames["metrics"]
    if metrics.empty:
        raise SystemExit(
            f"No metrics rows available after suite filter '{suite_id}'. "
            f"Check SuiteId values in {analysis_dir / 'metrics_enriched.csv'}."
        )

    generated = generate_experiment_figures(
        metrics=metrics,
        output_dir=output_dir,
        formats=formats,
        dpi=args.dpi,
        pd=pd,
        sns=sns,
        plt=plt,
        np=np,
    )
    write_figure_index(output_dir, generated)
    print(f"Wrote experiment figures to: {output_dir}")


if __name__ == "__main__":
    main()
