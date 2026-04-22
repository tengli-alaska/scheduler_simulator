#!/usr/bin/env python3
"""Generate run-level JSON manifests from metrics CSV outputs."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import subprocess
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_RUNS_DIR = ROOT_DIR / "runs"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def maybe_git_commit(root_dir: Path) -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root_dir, stderr=subprocess.DEVNULL
        )
        return out.decode("utf-8").strip()
    except Exception:
        return "unknown"


def first_row(path: Path) -> dict | None:
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        return next(reader, None)


def numeric_set(rows: list[dict], key: str) -> list[float]:
    vals = []
    for row in rows:
        raw = row.get(key)
        if raw is None or raw == "":
            continue
        try:
            vals.append(float(raw))
        except ValueError:
            continue
    return sorted(set(vals))


def collect_rows(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def build_manifest(metrics_path: Path, root_dir: Path) -> dict:
    rows = collect_rows(metrics_path)
    if not rows:
        raise ValueError(f"{metrics_path} has no data rows")

    first = rows[0]
    task_path = metrics_path.with_name("tasks.csv")

    manifest = {
        "manifest_schema_version": "manifest.v1",
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "git_commit": maybe_git_commit(root_dir),
        "benchmark_metadata": {
            "benchmark_version": first.get("BenchmarkVersion", "unknown"),
            "schema_version": first.get("SchemaVersion", "metrics.v2"),
            "suite_id": first.get("SuiteId", "unknown-suite"),
            "run_id": first.get("RunId", metrics_path.stem),
        },
        "source_files": {
            "metrics_csv": str(metrics_path.relative_to(root_dir)),
            "metrics_sha256": sha256_file(metrics_path),
            "tasks_csv": str(task_path.relative_to(root_dir)) if task_path.exists() else None,
            "tasks_sha256": sha256_file(task_path) if task_path.exists() else None,
        },
        "run_summary": {
            "rows": len(rows),
            "replications": numeric_set(rows, "Replication"),
            "num_tasks": numeric_set(rows, "NumTasks"),
            "cores": numeric_set(rows, "Cores"),
            "stop_time": numeric_set(rows, "StopTime"),
            "topologies": sorted(set(r.get("Topology", "") for r in rows if r.get("Topology"))),
            "balancers": sorted(set(r.get("Balancer", "") for r in rows if r.get("Balancer"))),
            "work_stealing_values": sorted(
                set(r.get("WorkStealing", "") for r in rows if r.get("WorkStealing"))
            ),
            "schedulers": sorted(set(r.get("Scheduler", "") for r in rows if r.get("Scheduler"))),
            "workloads": sorted(set(r.get("Workload", "") for r in rows if r.get("Workload"))),
        },
    }
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate JSON manifests for run artifacts")
    parser.add_argument("--runs-dir", default=str(DEFAULT_RUNS_DIR))
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    runs_dir = Path(args.runs_dir).resolve()

    metrics_paths = []
    for path in runs_dir.rglob("*.csv"):
        # Ignore helper dirs
        if "logs" in path.parts or "manifests" in path.parts:
            continue
        if path.name == "metrics.csv":
            metrics_paths.append(path)

    metrics_paths = sorted(metrics_paths)
    if not metrics_paths:
        raise SystemExit(f"No structured metrics.csv files found under {runs_dir}.")

    written = 0
    for metrics_path in metrics_paths:
        manifest = build_manifest(metrics_path, ROOT_DIR)
        out_path = metrics_path.parent / "manifest.json"
        with out_path.open("w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, sort_keys=True)
            f.write("\n")
        written += 1

    print(f"Wrote {written} manifests")


if __name__ == "__main__":
    main()
