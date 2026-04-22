#!/usr/bin/env python3
"""Export a lightweight benchmark summary table from analysis outputs.

This script is intentionally dependency-light (stdlib only). It produces:
  - analysis/summary_table.csv
  - analysis/summary_table.md

Rows are grouped by (ExperimentId, Workload, Scheduler) and report mean values.
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def to_float(value, default=0.0) -> float:
    try:
        return float(value)
    except Exception:
        return default


def split_experiments(raw: str) -> list[str]:
    parts = [p.strip() for p in str(raw).split(";") if p.strip()]
    clean = [p for p in parts if p.lower() != "none"]
    return clean or ["none"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export concise benchmark summary tables.")
    parser.add_argument("--analysis-dir", default="analysis")
    parser.add_argument("--output-csv", default="")
    parser.add_argument("--output-md", default="")
    return parser.parse_args()


def markdown_table(rows: list[dict], columns: list[str]) -> str:
    if not rows:
        return "_No rows._"
    head = "| " + " | ".join(columns) + " |"
    sep = "| " + " | ".join("---" for _ in columns) + " |"
    lines = [head, sep]
    for row in rows:
        vals = [str(row.get(c, "")).replace("|", "\\|") for c in columns]
        lines.append("| " + " | ".join(vals) + " |")
    return "\n".join(lines)


def main() -> None:
    args = parse_args()
    analysis_dir = Path(args.analysis_dir).resolve()
    metrics_path = analysis_dir / "metrics_enriched.csv"
    if not metrics_path.exists():
        raise SystemExit(f"Missing required file: {metrics_path}")

    out_csv = Path(args.output_csv).resolve() if args.output_csv else analysis_dir / "summary_table.csv"
    out_md = Path(args.output_md).resolve() if args.output_md else analysis_dir / "summary_table.md"

    with metrics_path.open("r", encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))

    agg = defaultdict(lambda: {"n": 0, "Throughput": 0.0, "MeanRT": 0.0, "JainsFairness": 0.0, "ContextSwitchesPerTask": 0.0, "CompletionRatio": 0.0})

    for row in rows:
        workload = str(row.get("Workload", "")).strip()
        scheduler = str(row.get("Scheduler", "")).strip()
        exp_ids = split_experiments(row.get("ExperimentIds") or "none")
        for exp_id in exp_ids:
            key = (exp_id, workload, scheduler)
            slot = agg[key]
            slot["n"] += 1
            slot["Throughput"] += to_float(row.get("Throughput", 0.0))
            slot["MeanRT"] += to_float(row.get("MeanRT", 0.0))
            slot["JainsFairness"] += to_float(row.get("JainsFairness", 0.0))
            slot["ContextSwitchesPerTask"] += to_float(row.get("ContextSwitchesPerTask", 0.0))
            slot["CompletionRatio"] += to_float(row.get("CompletionRatio", 0.0))

    out_rows = []
    for (exp_id, workload, scheduler), vals in sorted(agg.items()):
        n = max(1, vals["n"])
        out_rows.append(
            {
                "ExperimentId": exp_id,
                "Workload": workload,
                "Scheduler": scheduler,
                "Samples": vals["n"],
                "ThroughputMean": f"{vals['Throughput'] / n:.6g}",
                "MeanRTMean": f"{vals['MeanRT'] / n:.6g}",
                "FairnessMean": f"{vals['JainsFairness'] / n:.6g}",
                "ContextSwitchesPerTaskMean": f"{vals['ContextSwitchesPerTask'] / n:.6g}",
                "CompletionRatioMean": f"{vals['CompletionRatio'] / n:.6g}",
            }
        )

    fieldnames = [
        "ExperimentId",
        "Workload",
        "Scheduler",
        "Samples",
        "ThroughputMean",
        "MeanRTMean",
        "FairnessMean",
        "ContextSwitchesPerTaskMean",
        "CompletionRatioMean",
    ]
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with out_csv.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(out_rows)

    out_md.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Benchmark Summary Table", "", markdown_table(out_rows, fieldnames), ""]
    out_md.write_text("\n".join(lines), encoding="utf-8")

    print(f"Wrote summary CSV: {out_csv}")
    print(f"Wrote summary MD:  {out_md}")


if __name__ == "__main__":
    main()
