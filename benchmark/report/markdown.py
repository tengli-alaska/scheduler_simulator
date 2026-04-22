from __future__ import annotations

import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def _git_commit_hash(root_dir: Path) -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=root_dir,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return out.strip()
    except Exception:
        return "unknown"


def _to_rel_path(path: Path, base: Path) -> str:
    try:
        rel = path.relative_to(base)
        return rel.as_posix()
    except ValueError:
        return Path(os.path.relpath(path, base)).as_posix()


def _markdown_table(rows: list[dict], columns: list[str]) -> str:
    if not rows:
        return "_No rows._"

    header = "| " + " | ".join(columns) + " |"
    sep = "| " + " | ".join("---" for _ in columns) + " |"
    lines = [header, sep]
    for row in rows:
        vals = []
        for col in columns:
            val = str(row.get(col, ""))
            vals.append(val.replace("\n", " ").replace("|", "\\|"))
        lines.append("| " + " | ".join(vals) + " |")
    return "\n".join(lines)


def _build_environment_rows(pd, sns, plt, commit_hash: str) -> list[dict]:
    return [
        {"Key": "GeneratedAtUtc", "Value": datetime.now(timezone.utc).isoformat()},
        {"Key": "CommitHash", "Value": commit_hash},
        {"Key": "PythonVersion", "Value": sys.version.split("\n")[0]},
        {"Key": "Platform", "Value": platform.platform()},
        {"Key": "PandasVersion", "Value": getattr(pd, "__version__", "unknown")},
        {"Key": "SeabornVersion", "Value": getattr(sns, "__version__", "unknown")},
        {"Key": "MatplotlibVersion", "Value": getattr(plt.matplotlib, "__version__", "unknown")},
    ]


def build_markdown_report(
    *,
    root_dir: Path,
    suite_id: str,
    report_path: Path,
    generated_figures: list[dict],
    frames: dict,
    pd,
    sns,
    plt,
) -> Path:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    commit_hash = _git_commit_hash(root_dir)

    run_index = frames.get("run_index")
    quality_checks = frames.get("quality_checks")

    config_cols = [
        "SuiteId",
        "RunId",
        "Experiments",
        "Rows",
        "NumTasks",
        "Cores",
        "StopTime",
        "Topologies",
        "Balancers",
        "WorkStealingValues",
        "Workloads",
        "Schedulers",
        "ExecutionStatus",
    ]

    config_rows: list[dict] = []
    if run_index is not None and not run_index.empty:
        for col in config_cols:
            if col not in run_index.columns:
                run_index[col] = ""
        run_index_sorted = run_index.sort_values(["SuiteId", "RunId"])
        config_rows = run_index_sorted[config_cols].to_dict(orient="records")

    qc_summary_rows: list[dict] = []
    if quality_checks is not None and not quality_checks.empty:
        for col in ["CheckType", "Severity"]:
            if col not in quality_checks.columns:
                quality_checks[col] = ""
        summary = (
            quality_checks.groupby(["CheckType", "Severity"], dropna=False)
            .size()
            .reset_index(name="Count")
            .sort_values(["CheckType", "Severity"])
        )
        qc_summary_rows = summary.to_dict(orient="records")

    env_rows = _build_environment_rows(pd, sns, plt, commit_hash)

    lines = []
    lines.append(f"# Scheduler Benchmark Report ({suite_id})")
    lines.append("")
    lines.append("## Publication Figures")
    lines.append("")
    if not generated_figures:
        lines.append("No publication figures were generated for this suite filter.")
    else:
        for fig in generated_figures:
            title = fig["title"]
            lines.append(f"### {title}")
            lines.append("")

            png = next((Path(p) for p in fig["files"] if str(p).lower().endswith(".png")), None)
            if png is not None:
                rel_png = _to_rel_path(png, report_path.parent)
                lines.append(f"![{title}]({rel_png})")
                lines.append("")

            artifact_links = []
            for p in fig["files"]:
                rel = _to_rel_path(Path(p), report_path.parent)
                artifact_links.append(f"[`{Path(p).name}`]({rel})")
            if artifact_links:
                lines.append("Artifacts: " + ", ".join(artifact_links))
                lines.append("")

    lines.append("## Reproducibility Appendix")
    lines.append("")
    lines.append("### Configuration Table")
    lines.append("")
    lines.append(_markdown_table(config_rows, config_cols))
    lines.append("")
    lines.append("### Environment + Commit")
    lines.append("")
    lines.append(_markdown_table(env_rows, ["Key", "Value"]))
    lines.append("")
    lines.append("### Quality Check Summary")
    lines.append("")
    lines.append(_markdown_table(qc_summary_rows, ["CheckType", "Severity", "Count"]))
    lines.append("")

    report_path.write_text("\n".join(lines), encoding="utf-8")
    return report_path
