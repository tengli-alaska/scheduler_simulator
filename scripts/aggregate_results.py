#!/usr/bin/env python3
"""Aggregate structured benchmark artifacts into canonical analysis outputs."""

from __future__ import annotations

import csv
import json
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
RUNS_DIR = ROOT_DIR / "runs"
ANALYSIS_DIR = ROOT_DIR / "analysis"
SPEC_DIR = ROOT_DIR / "benchmark" / "spec" / "suites"

ANALYSIS_DIR.mkdir(parents=True, exist_ok=True)

KNOWN_ALL_WORKLOADS = {"Desktop", "Server", "GoogleTraceV3", "AlibabaTraceV2018"}
WORKLOAD_KEY_TO_NAME = {
    "desktop": "Desktop",
    "server": "Server",
    "google": "GoogleTraceV3",
    "alibaba": "AlibabaTraceV2018",
}
EXP_NUM_RE = re.compile(r"^exp(\d+)")


def to_float(value, default=0.0):
    try:
        return float(value)
    except Exception:
        return default


def to_int(value, default=0):
    try:
        return int(float(value))
    except Exception:
        return default


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stddev(values):
    return statistics.pstdev(values) if len(values) > 1 else 0.0


def cv(values):
    m = mean(values)
    return (stddev(values) / m) if m > 0 else 0.0


def safe_div(a, b):
    return a / b if b else 0.0


def approx_eq(a, b, tol=1e-6):
    return abs(float(a) - float(b)) <= tol


def corr(xs, ys):
    if len(xs) < 2 or len(ys) < 2 or len(xs) != len(ys):
        return 0.0
    mx = mean(xs)
    my = mean(ys)
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    den_x = math.sqrt(sum((x - mx) ** 2 for x in xs))
    den_y = math.sqrt(sum((y - my) ** 2 for y in ys))
    den = den_x * den_y
    return (num / den) if den else 0.0


def read_csv_rows(path: Path):
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_json(path: Path):
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def canonical_workload_name(workload_key: str) -> str:
    return WORKLOAD_KEY_TO_NAME.get(workload_key.lower(), workload_key)


def discover_paths(runs_dir: Path):
    metrics = []
    tasks = []
    for path in runs_dir.rglob("*.csv"):
        if "logs" in path.parts or "manifests" in path.parts:
            continue
        if path.name == "metrics.csv":
            metrics.append(path)
        elif path.name == "tasks.csv":
            tasks.append(path)
    return sorted(metrics), sorted(tasks)


def list_suite_specs(spec_dir: Path):
    out = {}
    if not spec_dir.exists():
        return out

    for path in sorted(spec_dir.glob("*.json")):
        spec = load_json(path)
        if not isinstance(spec, dict):
            continue
        suite_id = spec.get("suite_id")
        if not suite_id:
            continue

        exp_by_id = {exp["id"]: exp for exp in spec.get("experiments", []) if "id" in exp}
        memo = {}

        def resolved_runs(exp_id):
            if exp_id in memo:
                return memo[exp_id]
            exp = exp_by_id.get(exp_id, {})
            runs = list(exp.get("runs", []))
            parent = exp.get("reuse_from_experiment")
            if parent:
                runs.extend(resolved_runs(parent))
            memo[exp_id] = runs
            return runs

        experiments = []
        for exp in spec.get("experiments", []):
            exp_id = exp.get("id")
            if not exp_id:
                continue
            normalized_runs = []
            for run in resolved_runs(exp_id):
                topology = run.get("topology", spec.get("defaults", {}).get("topology", "sq"))
                balancer = run.get("balancer", "leastloaded") if topology == "mq" else "na"
                ws = run.get("work_stealing", True) if topology == "mq" else "na"
                normalized_runs.append(
                    {
                        "workload": str(run.get("workload", "")).lower(),
                        "cores": int(run.get("cores", 0)),
                        "num_tasks": int(run.get("num_tasks", 0)),
                        "stop_time": float(run.get("stop_time", 0.0)),
                        "topology": topology,
                        "balancer": balancer,
                        "work_stealing": "on" if ws is True else ("off" if ws is False else "na"),
                    }
                )
            experiments.append({"id": exp_id, "runs": normalized_runs})

        out[suite_id] = {
            "path": str(path.relative_to(ROOT_DIR)),
            "benchmark_version": spec.get("benchmark_version", ""),
            "experiments": experiments,
        }
    return out


def load_execution_summary_index(runs_dir: Path):
    """Map (suite_id, run_id) -> execution metadata from execution summaries."""
    manifests_dir = runs_dir / "manifests"
    out = {}
    if not manifests_dir.exists():
        return out

    for path in sorted(manifests_dir.glob("*.execution_summary.json")):
        doc = load_json(path)
        if not isinstance(doc, dict):
            continue
        suite_id = doc.get("suite_id", "")
        if not suite_id:
            continue
        for case in doc.get("cases", []):
            run_id = case.get("run_id", "")
            if not run_id:
                continue
            out[(suite_id, run_id)] = {
                "experiments": list(case.get("experiments", [])),
                "status": case.get("status", ""),
                "log_path": case.get("log_path", ""),
                "summary_path": str(path.relative_to(ROOT_DIR)),
            }
    return out


def normalize_summary_values(run_summary: dict, key: str):
    vals = run_summary.get(key, [])
    if vals is None:
        return []
    if not isinstance(vals, list):
        return [vals]
    return vals


def run_def_matches_summary(run_def: dict, run_summary: dict):
    workloads = set(normalize_summary_values(run_summary, "workloads"))
    cores = {int(v) for v in normalize_summary_values(run_summary, "cores")}
    num_tasks = {int(v) for v in normalize_summary_values(run_summary, "num_tasks")}
    stop_times = [float(v) for v in normalize_summary_values(run_summary, "stop_time")]
    topologies = {str(v).lower() for v in normalize_summary_values(run_summary, "topologies")}
    balancers = {str(v).lower() for v in normalize_summary_values(run_summary, "balancers")}
    ws_vals = {str(v).lower() for v in normalize_summary_values(run_summary, "work_stealing_values")}

    if run_def["workload"] == "all":
        if len(workloads) < 2:
            return False
    else:
        expected_name = canonical_workload_name(run_def["workload"])
        if expected_name not in workloads:
            return False

    if run_def["cores"] not in cores:
        return False
    if run_def["num_tasks"] not in num_tasks:
        return False
    if not any(approx_eq(run_def["stop_time"], st) for st in stop_times):
        return False
    if run_def["topology"] not in topologies:
        return False

    if run_def["topology"] == "mq":
        if run_def["balancer"] not in balancers:
            return False
        if run_def["work_stealing"] not in ws_vals:
            return False

    return True


def infer_experiments_from_spec(suite_id: str, run_summary: dict, suite_specs: dict):
    suite = suite_specs.get(suite_id)
    if not suite:
        return []
    matched = []
    for exp in suite.get("experiments", []):
        exp_id = exp["id"]
        for run_def in exp.get("runs", []):
            if run_def_matches_summary(run_def, run_summary):
                matched.append(exp_id)
                break
    return sorted(set(matched))


def summarize_run_rows(rows: list[dict]):
    return {
        "rows": len(rows),
        "replications": sorted({to_int(r.get("Replication", 0)) for r in rows}),
        "num_tasks": sorted({to_int(r.get("NumTasks", 0)) for r in rows}),
        "cores": sorted({to_int(r.get("Cores", 0)) for r in rows}),
        "stop_time": sorted({to_float(r.get("StopTime", 0.0)) for r in rows}),
        "topologies": sorted({str(r.get("Topology", "")) for r in rows if r.get("Topology")}),
        "balancers": sorted({str(r.get("Balancer", "")) for r in rows if r.get("Balancer")}),
        "work_stealing_values": sorted(
            {str(r.get("WorkStealing", "")) for r in rows if r.get("WorkStealing")}
        ),
        "schedulers": sorted({str(r.get("Scheduler", "")) for r in rows if r.get("Scheduler")}),
        "workloads": sorted({str(r.get("Workload", "")) for r in rows if r.get("Workload")}),
    }


def topology_variant(row):
    topology = str(row.get("Topology", "")).lower()
    cores = to_int(row.get("Cores", 0))
    balancer = str(row.get("Balancer", "na")).lower()
    steal = str(row.get("WorkStealing", "na")).lower()

    if topology == "sq":
        return f"SQ-{cores}c"
    if topology == "mq":
        lb = "RR" if balancer == "rr" else ("LL" if balancer == "leastloaded" else balancer.upper())
        ws = "WS-on" if steal == "on" else ("WS-off" if steal == "off" else "WS-na")
        return f"MQ-{lb}-{cores}c-{ws}"
    return f"{topology.upper()}-{cores}c"


def experiment_flags(experiment_ids: list[str]):
    flags = {f"Exp{i}": 0 for i in range(1, 7)}
    for exp_id in experiment_ids:
        m = EXP_NUM_RE.match(exp_id.lower())
        if not m:
            continue
        idx = int(m.group(1))
        if 1 <= idx <= 6:
            flags[f"Exp{idx}"] = 1
    return flags


def write_csv(path: Path, fieldnames: list[str], rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def list_join(values):
    return ";".join(str(v) for v in values)


def add_quality_issue(
    quality_rows,
    check_type,
    severity,
    suite_id,
    run_id,
    source_file,
    scheduler,
    workload,
    metric,
    value,
    threshold,
    message,
):
    quality_rows.append(
        {
            "CheckType": check_type,
            "Severity": severity,
            "SuiteId": suite_id,
            "RunId": run_id,
            "SourceFile": source_file,
            "Scheduler": scheduler,
            "Workload": workload,
            "Metric": metric,
            "Value": value,
            "Threshold": threshold,
            "Message": message,
        }
    )


metrics_paths, task_paths = discover_paths(RUNS_DIR)

if not metrics_paths:
    raise SystemExit(
        "No structured metrics files found in runs/<suite>/<run>/metrics.csv.\n"
        "Run: python3 -m benchmark.runner.run_suite --suite benchmark/spec/suites/community_v1.json"
    )

suite_specs = list_suite_specs(SPEC_DIR)
execution_idx = load_execution_summary_index(RUNS_DIR)

metrics_rows = []
run_index_rows = []
quality_rows = []

# Map (suite_id, run_id) -> experiment ids string for downstream task-derived outputs.
run_experiments_map = {}

for metrics_path in metrics_paths:
    raw_rows = read_csv_rows(metrics_path)
    if not raw_rows:
        continue

    rel_metrics = str(metrics_path.relative_to(ROOT_DIR))
    rel_tasks = str((metrics_path.parent / "tasks.csv").relative_to(ROOT_DIR))
    manifest_path = metrics_path.parent / "manifest.json"
    manifest = load_json(manifest_path) if manifest_path.exists() else None

    first = raw_rows[0]
    suite_id = str(
        first.get("SuiteId")
        or ((manifest or {}).get("benchmark_metadata", {}).get("suite_id"))
        or metrics_path.parent.parent.name
    )
    run_id = str(
        first.get("RunId")
        or ((manifest or {}).get("benchmark_metadata", {}).get("run_id"))
        or metrics_path.parent.name
    )
    benchmark_version = str(
        first.get("BenchmarkVersion")
        or ((manifest or {}).get("benchmark_metadata", {}).get("benchmark_version")
        or "unknown")
    )
    schema_version = str(first.get("SchemaVersion") or "metrics.v2")

    run_summary = (manifest or {}).get("run_summary") or summarize_run_rows(raw_rows)

    exec_meta = execution_idx.get((suite_id, run_id), {})
    experiment_ids = list(exec_meta.get("experiments", []))
    if not experiment_ids:
        experiment_ids = infer_experiments_from_spec(suite_id, run_summary, suite_specs)
    experiment_ids = sorted(set(experiment_ids))
    experiment_ids_str = ";".join(experiment_ids) if experiment_ids else "none"
    exp_flags = experiment_flags(experiment_ids)
    run_experiments_map[(suite_id, run_id)] = experiment_ids_str

    run_index_rows.append(
        {
            "BenchmarkVersion": benchmark_version,
            "SuiteId": suite_id,
            "RunId": run_id,
            "MetricsSchemaVersion": schema_version,
            "MetricsPath": rel_metrics,
            "TasksPath": rel_tasks if (ROOT_DIR / rel_tasks).exists() else "",
            "ManifestPath": str(manifest_path.relative_to(ROOT_DIR)) if manifest_path.exists() else "",
            "ExecutionSummaryPath": exec_meta.get("summary_path", ""),
            "ExecutionStatus": exec_meta.get("status", ""),
            "ExecutionLogPath": exec_meta.get("log_path", ""),
            "Experiments": experiment_ids_str,
            "Rows": int(run_summary.get("rows", len(raw_rows))),
            "Replications": list_join(run_summary.get("replications", [])),
            "NumTasks": list_join(run_summary.get("num_tasks", [])),
            "Cores": list_join(run_summary.get("cores", [])),
            "StopTime": list_join(run_summary.get("stop_time", [])),
            "Topologies": list_join(run_summary.get("topologies", [])),
            "Balancers": list_join(run_summary.get("balancers", [])),
            "WorkStealingValues": list_join(run_summary.get("work_stealing_values", [])),
            "Schedulers": list_join(run_summary.get("schedulers", [])),
            "Workloads": list_join(run_summary.get("workloads", [])),
            "ManifestGitCommit": ((manifest or {}).get("git_commit", "")),
            "ManifestGeneratedAtUtc": ((manifest or {}).get("generated_at_utc", "")),
        }
    )

    required_text = ["BenchmarkVersion", "SchemaVersion", "SuiteId", "RunId", "Scheduler", "Workload", "Topology"]
    required_numeric = [
        "Completed",
        "CompletionRatio",
        "MeanRT",
        "P99RT",
        "Throughput",
        "JainsFairness",
        "ContextSwitches",
        "Preemptions",
    ]

    for raw in raw_rows:
        source_file = rel_metrics
        scheduler = raw.get("Scheduler", "")
        workload = raw.get("Workload", "")

        for col in required_text:
            if str(raw.get(col, "")).strip() == "":
                add_quality_issue(
                    quality_rows,
                    "missing_cell",
                    "ERROR",
                    suite_id,
                    run_id,
                    source_file,
                    scheduler,
                    workload,
                    col,
                    "",
                    "required_non_empty",
                    f"Missing required text field: {col}",
                )
        for col in required_numeric:
            raw_val = str(raw.get(col, "")).strip()
            if raw_val == "":
                add_quality_issue(
                    quality_rows,
                    "missing_cell",
                    "ERROR",
                    suite_id,
                    run_id,
                    source_file,
                    scheduler,
                    workload,
                    col,
                    "",
                    "required_numeric",
                    f"Missing required numeric field: {col}",
                )
            else:
                try:
                    float(raw_val)
                except ValueError:
                    add_quality_issue(
                        quality_rows,
                        "missing_cell",
                        "ERROR",
                        suite_id,
                        run_id,
                        source_file,
                        scheduler,
                        workload,
                        col,
                        raw_val,
                        "required_numeric",
                        f"Invalid numeric value in {col}",
                    )

        row = {}
        row["BenchmarkVersion"] = benchmark_version
        row["SchemaVersion"] = schema_version
        row["SuiteId"] = suite_id
        row["RunId"] = run_id
        row["Seed"] = to_int(raw.get("Seed", 0))
        row["SourceFile"] = source_file
        row["Replication"] = to_int(raw.get("Replication", 0))
        row["NumTasks"] = to_int(raw.get("NumTasks", 0))
        row["Cores"] = to_int(raw.get("Cores", 0))
        row["Topology"] = raw.get("Topology", "")
        row["Balancer"] = raw.get("Balancer", "")
        row["WorkStealing"] = raw.get("WorkStealing", "")
        row["StopTime"] = to_float(raw.get("StopTime", 0.0))
        row["Scheduler"] = scheduler
        row["SchedulerClass"] = (
            "fairness_oriented" if scheduler in {"CFS", "EEVDF", "Stride"} else "alternative"
        )
        row["Workload"] = workload
        row["WorkloadType"] = "synthetic" if workload in {"Server", "Desktop"} else "real_trace"
        row["TopologyVariant"] = topology_variant(row)
        row["ExperimentIds"] = experiment_ids_str
        row.update(exp_flags)

        row["Completed"] = to_int(raw.get("Completed", 0))
        row["CompletionRatio"] = to_float(raw.get("CompletionRatio", 0))
        row["MeanRT"] = to_float(raw.get("MeanRT", 0))
        row["P95RT"] = to_float(raw.get("P95RT", 0))
        row["P99RT"] = to_float(raw.get("P99RT", 0))
        row["MeanTAT"] = to_float(raw.get("MeanTAT", 0))
        row["MeanWT"] = to_float(raw.get("MeanWT", 0))
        row["Throughput"] = to_float(raw.get("Throughput", 0))
        row["ThroughputPerCore"] = to_float(raw.get("ThroughputPerCore", 0))
        row["Utilization"] = to_float(raw.get("Utilization", 0))
        row["JainsFairness"] = to_float(raw.get("JainsFairness", 0))
        row["ContextSwitches"] = to_int(raw.get("ContextSwitches", 0))
        row["Preemptions"] = to_int(raw.get("Preemptions", 0))
        row["ContextSwitchesPerTask"] = safe_div(row["ContextSwitches"], row["Completed"])
        row["PreemptionsPerTask"] = safe_div(row["Preemptions"], row["Completed"])

        completion = row["CompletionRatio"]
        if completion < 0.01:
            add_quality_issue(
                quality_rows,
                "low_completion_ratio",
                "WARN",
                suite_id,
                run_id,
                source_file,
                scheduler,
                workload,
                "CompletionRatio",
                completion,
                "<0.01",
                "Extremely low completion ratio; latency/overhead can be unstable.",
            )
        elif completion < 0.10:
            add_quality_issue(
                quality_rows,
                "low_completion_ratio",
                "INFO",
                suite_id,
                run_id,
                source_file,
                scheduler,
                workload,
                "CompletionRatio",
                completion,
                "<0.10",
                "Low completion ratio; interpret aggregate metrics with caution.",
            )

        for metric_name in ["MeanRT", "P99RT", "ContextSwitchesPerTask"]:
            value = row.get(metric_name, 0.0)
            if not (isinstance(value, (int, float)) and math.isfinite(value) and value > 0):
                add_quality_issue(
                    quality_rows,
                    "invalid_log_scale_candidate",
                    "WARN",
                    suite_id,
                    run_id,
                    source_file,
                    scheduler,
                    workload,
                    metric_name,
                    value,
                    ">0",
                    "Value is non-positive or invalid for log-scale plotting.",
                )

        metrics_rows.append(row)


dup_map = defaultdict(list)
for row in metrics_rows:
    key = (
        row["SuiteId"],
        row["RunId"],
        row["Seed"],
        row["Replication"],
        row["Scheduler"],
        row["Workload"],
        row["Topology"],
        row["Balancer"],
        row["WorkStealing"],
        row["Cores"],
        row["NumTasks"],
        row["StopTime"],
    )
    dup_map[key].append(row)

for key, rows in dup_map.items():
    if len(rows) <= 1:
        continue
    sample = rows[0]
    add_quality_issue(
        quality_rows,
        "duplicate_config",
        "WARN",
        sample["SuiteId"],
        sample["RunId"],
        sample["SourceFile"],
        sample["Scheduler"],
        sample["Workload"],
        "config_key",
        len(rows),
        "count=1",
        "Duplicate configuration rows detected within aggregated metrics.",
    )


metrics_out_fields = [
    "BenchmarkVersion",
    "SchemaVersion",
    "SuiteId",
    "RunId",
    "Seed",
    "SourceFile",
    "Replication",
    "NumTasks",
    "Cores",
    "Topology",
    "Balancer",
    "WorkStealing",
    "StopTime",
    "Scheduler",
    "SchedulerClass",
    "Workload",
    "WorkloadType",
    "TopologyVariant",
    "ExperimentIds",
    "Exp1",
    "Exp2",
    "Exp3",
    "Exp4",
    "Exp5",
    "Exp6",
    "Completed",
    "CompletionRatio",
    "MeanRT",
    "P95RT",
    "P99RT",
    "MeanTAT",
    "MeanWT",
    "Throughput",
    "ThroughputPerCore",
    "Utilization",
    "JainsFairness",
    "ContextSwitches",
    "Preemptions",
    "ContextSwitchesPerTask",
    "PreemptionsPerTask",
]

run_index_fields = [
    "BenchmarkVersion",
    "SuiteId",
    "RunId",
    "MetricsSchemaVersion",
    "MetricsPath",
    "TasksPath",
    "ManifestPath",
    "ExecutionSummaryPath",
    "ExecutionStatus",
    "ExecutionLogPath",
    "Experiments",
    "Rows",
    "Replications",
    "NumTasks",
    "Cores",
    "StopTime",
    "Topologies",
    "Balancers",
    "WorkStealingValues",
    "Schedulers",
    "Workloads",
    "ManifestGitCommit",
    "ManifestGeneratedAtUtc",
]

quality_fields = [
    "CheckType",
    "Severity",
    "SuiteId",
    "RunId",
    "SourceFile",
    "Scheduler",
    "Workload",
    "Metric",
    "Value",
    "Threshold",
    "Message",
]

write_csv(ANALYSIS_DIR / "metrics_enriched.csv", metrics_out_fields, metrics_rows)
write_csv(ANALYSIS_DIR / "run_index.csv", run_index_fields, run_index_rows)
write_csv(ANALYSIS_DIR / "quality_checks.csv", quality_fields, quality_rows)


# --------------------------- Task-derived analysis ----------------------------

task_rows = []
for path in task_paths:
    raw_rows = read_csv_rows(path)
    rel_source = str(path.relative_to(ROOT_DIR))
    for raw in raw_rows:
        suite_id = str(raw.get("SuiteId") or path.parent.parent.name)
        run_id = str(raw.get("RunId") or path.parent.name)
        row = {
            "SourceFile": rel_source,
            "BenchmarkVersion": str(raw.get("BenchmarkVersion", "unknown")),
            "SchemaVersion": str(raw.get("SchemaVersion", "tasks.v2")),
            "SuiteId": suite_id,
            "RunId": run_id,
            "Seed": to_int(raw.get("Seed", 0)),
            "Replication": to_int(raw.get("Replication", 0)),
            "NumTasks": to_int(raw.get("NumTasks", 0)),
            "Cores": to_int(raw.get("Cores", 0)),
            "Topology": raw.get("Topology", ""),
            "Balancer": raw.get("Balancer", ""),
            "WorkStealing": raw.get("WorkStealing", ""),
            "StopTime": to_float(raw.get("StopTime", 0.0)),
            "Scheduler": raw.get("Scheduler", ""),
            "Workload": raw.get("Workload", ""),
            "TaskID": to_int(raw.get("TaskID", 0)),
            "Nice": to_int(raw.get("Nice", 0)),
            "Weight": to_float(raw.get("Weight", 0.0)),
            "Arrival": to_float(raw.get("Arrival", 0.0)),
            "Execution": to_float(raw.get("Execution", 0.0)),
            "AllocatedCPU": to_float(raw.get("AllocatedCPU", 0.0)),
            "Remaining": to_float(raw.get("Remaining", 0.0)),
            "Started": to_int(raw.get("Started", 0)),
            "Completed": to_int(raw.get("Completed", 0)),
            "StartTime": to_float(raw.get("StartTime", 0.0)),
            "CompletionTime": to_float(raw.get("CompletionTime", 0.0)),
            "ResponseTime": to_float(raw.get("ResponseTime", 0.0)),
            "TurnaroundTime": to_float(raw.get("TurnaroundTime", 0.0)),
            "WaitTime": to_float(raw.get("WaitTime", 0.0)),
            "PreemptionCount": to_int(raw.get("PreemptionCount", 0)),
        }
        task_rows.append(row)


run_key_fields = [
    "SuiteId",
    "RunId",
    "Seed",
    "Replication",
    "NumTasks",
    "Cores",
    "Topology",
    "Balancer",
    "WorkStealing",
    "StopTime",
    "Scheduler",
    "Workload",
]


def run_key(row):
    return tuple(row.get(k) for k in run_key_fields)


tasks_by_run = defaultdict(list)
for row in task_rows:
    tasks_by_run[run_key(row)].append(row)

allocation_rows = []
nice_bucket_rows = []
workload_char_rows = []

for key, tasks in tasks_by_run.items():
    (
        suite_id,
        run_id,
        seed,
        replication,
        num_tasks,
        cores,
        topology,
        balancer,
        work_stealing,
        stop_time,
        scheduler,
        workload,
    ) = key

    total_alloc = sum(t["AllocatedCPU"] for t in tasks)
    total_weight = sum(t["Weight"] for t in tasks)
    completed_ratio = safe_div(sum(t["Completed"] for t in tasks), len(tasks))

    share_abs_err = []
    ratio_vals = []
    for t in tasks:
        obs = safe_div(t["AllocatedCPU"], total_alloc)
        exp = safe_div(t["Weight"], total_weight)
        share_abs_err.append(abs(obs - exp))
        ratio_vals.append(safe_div(obs, exp) if exp > 0 else 0.0)

    ratio_sq_sum = sum(v * v for v in ratio_vals)
    proportional_jain = safe_div((sum(ratio_vals) ** 2), (len(ratio_vals) * ratio_sq_sum)) if ratio_sq_sum else 0.0
    slowdowns = [safe_div(t["TurnaroundTime"], t["Execution"]) for t in tasks if t["Execution"] > 0]
    responses = [t["ResponseTime"] for t in tasks]
    weights = [t["Weight"] for t in tasks]
    weight_vs_slowdown_corr = corr(weights, slowdowns) if len(weights) == len(slowdowns) else 0.0
    weight_vs_response_corr = corr(weights, responses)

    exp_ids = run_experiments_map.get((suite_id, run_id), "none")
    exp_flags = experiment_flags([] if exp_ids == "none" else exp_ids.split(";"))

    allocation_rows.append(
        {
            "SuiteId": suite_id,
            "RunId": run_id,
            "Seed": seed,
            "Replication": replication,
            "NumTasks": num_tasks,
            "Cores": cores,
            "Topology": topology,
            "Balancer": balancer,
            "WorkStealing": work_stealing,
            "StopTime": stop_time,
            "Scheduler": scheduler,
            "Workload": workload,
            "ExperimentIds": exp_ids,
            **exp_flags,
            "CompletedRatioFromTasks": completed_ratio,
            "TotalAllocatedCPU": total_alloc,
            "TotalWeight": total_weight,
            "ShareMAE": mean(share_abs_err),
            "ShareTV": 0.5 * sum(share_abs_err),
            "ProportionalJain": proportional_jain,
            "WeightVsSlowdownCorr": weight_vs_slowdown_corr,
            "WeightVsResponseCorr": weight_vs_response_corr,
        }
    )

    by_nice = defaultdict(
        lambda: {
            "alloc": 0.0,
            "weight": 0.0,
            "count": 0,
            "resp_sum": 0.0,
            "wait_sum": 0.0,
            "tat_sum": 0.0,
            "slow_sum": 0.0,
        }
    )
    for t in tasks:
        n = t["Nice"]
        by_nice[n]["alloc"] += t["AllocatedCPU"]
        by_nice[n]["weight"] += t["Weight"]
        by_nice[n]["count"] += 1
        by_nice[n]["resp_sum"] += t["ResponseTime"]
        by_nice[n]["wait_sum"] += t["WaitTime"]
        by_nice[n]["tat_sum"] += t["TurnaroundTime"]
        by_nice[n]["slow_sum"] += safe_div(t["TurnaroundTime"], t["Execution"])

    for nice, vals in sorted(by_nice.items()):
        obs_share = safe_div(vals["alloc"], total_alloc)
        exp_share = safe_div(vals["weight"], total_weight)
        nice_bucket_rows.append(
            {
                "SuiteId": suite_id,
                "RunId": run_id,
                "Seed": seed,
                "Replication": replication,
                "NumTasks": num_tasks,
                "Cores": cores,
                "Topology": topology,
                "Balancer": balancer,
                "WorkStealing": work_stealing,
                "StopTime": stop_time,
                "Scheduler": scheduler,
                "Workload": workload,
                "ExperimentIds": exp_ids,
                **exp_flags,
                "Nice": nice,
                "TaskCount": vals["count"],
                "ObservedShare": obs_share,
                "ExpectedWeightShare": exp_share,
                "ShareDelta": obs_share - exp_share,
                "AvgResponseTime": safe_div(vals["resp_sum"], vals["count"]),
                "AvgWaitTime": safe_div(vals["wait_sum"], vals["count"]),
                "AvgTurnaroundTime": safe_div(vals["tat_sum"], vals["count"]),
                "AvgSlowdown": safe_div(vals["slow_sum"], vals["count"]),
            }
        )

    arrivals = sorted(t["Arrival"] for t in tasks)
    inter_arrivals = [arrivals[i] - arrivals[i - 1] for i in range(1, len(arrivals))]
    executions = [t["Execution"] for t in tasks]
    weights = [t["Weight"] for t in tasks]
    nices = [t["Nice"] for t in tasks]

    workload_char_rows.append(
        {
            "SuiteId": suite_id,
            "RunId": run_id,
            "Seed": seed,
            "Replication": replication,
            "NumTasks": num_tasks,
            "Cores": cores,
            "Topology": topology,
            "Balancer": balancer,
            "WorkStealing": work_stealing,
            "StopTime": stop_time,
            "Scheduler": scheduler,
            "Workload": workload,
            "ExperimentIds": exp_ids,
            **exp_flags,
            "ArrivalSpan": (arrivals[-1] - arrivals[0]) if arrivals else 0.0,
            "InterArrivalMean": mean(inter_arrivals),
            "InterArrivalCV": cv(inter_arrivals),
            "ExecutionMean": mean(executions),
            "ExecutionCV": cv(executions),
            "WeightMean": mean(weights),
            "WeightCV": cv(weights),
            "NiceStdDev": stddev(nices),
            "UniqueNice": len(set(nices)),
        }
    )


def write_rows(path: Path, rows):
    if not rows:
        return
    write_csv(path, list(rows[0].keys()), rows)


write_rows(ANALYSIS_DIR / "weighted_allocation_summary.csv", allocation_rows)
write_rows(ANALYSIS_DIR / "nice_bucket_allocation.csv", nice_bucket_rows)
write_rows(ANALYSIS_DIR / "workload_characteristics.csv", workload_char_rows)


baseline_map = {}
for row in metrics_rows:
    if row["Scheduler"] != "MLFQ":
        continue
    key = (
        row["SuiteId"],
        row["RunId"],
        row["Seed"],
        row["Replication"],
        row["NumTasks"],
        row["Cores"],
        row["Topology"],
        row["Balancer"],
        row["WorkStealing"],
        row["StopTime"],
        row["Workload"],
    )
    baseline_map[key] = row

chars_by_key = {}
for row in workload_char_rows:
    key = (
        row["SuiteId"],
        row["RunId"],
        row["Seed"],
        row["Replication"],
        row["NumTasks"],
        row["Cores"],
        row["Topology"],
        row["Balancer"],
        row["WorkStealing"],
        row["StopTime"],
        row["Scheduler"],
        row["Workload"],
    )
    chars_by_key[key] = row

fairness_vs_chars = []
for row in metrics_rows:
    if row["Scheduler"] not in {"CFS", "EEVDF", "Stride"}:
        continue

    baseline_key = (
        row["SuiteId"],
        row["RunId"],
        row["Seed"],
        row["Replication"],
        row["NumTasks"],
        row["Cores"],
        row["Topology"],
        row["Balancer"],
        row["WorkStealing"],
        row["StopTime"],
        row["Workload"],
    )
    baseline = baseline_map.get(baseline_key)
    char_key = (
        row["SuiteId"],
        row["RunId"],
        row["Seed"],
        row["Replication"],
        row["NumTasks"],
        row["Cores"],
        row["Topology"],
        row["Balancer"],
        row["WorkStealing"],
        row["StopTime"],
        row["Scheduler"],
        row["Workload"],
    )
    chars = chars_by_key.get(char_key, {})

    fairness_vs_chars.append(
        {
            "SuiteId": row["SuiteId"],
            "RunId": row["RunId"],
            "Seed": row["Seed"],
            "Replication": row["Replication"],
            "NumTasks": row["NumTasks"],
            "Cores": row["Cores"],
            "Topology": row["Topology"],
            "Balancer": row["Balancer"],
            "WorkStealing": row["WorkStealing"],
            "StopTime": row["StopTime"],
            "Scheduler": row["Scheduler"],
            "Workload": row["Workload"],
            "WorkloadType": row["WorkloadType"],
            "ExperimentIds": row["ExperimentIds"],
            "Exp1": row["Exp1"],
            "Exp2": row["Exp2"],
            "Exp3": row["Exp3"],
            "Exp4": row["Exp4"],
            "Exp5": row["Exp5"],
            "Exp6": row["Exp6"],
            "Throughput": row["Throughput"],
            "MeanRT": row["MeanRT"],
            "ThroughputDeltaVsMLFQ": (row["Throughput"] - baseline["Throughput"]) if baseline else 0.0,
            "MeanRTDeltaVsMLFQ": (row["MeanRT"] - baseline["MeanRT"]) if baseline else 0.0,
            "ExecutionCV": to_float(chars.get("ExecutionCV", 0.0)),
            "InterArrivalCV": to_float(chars.get("InterArrivalCV", 0.0)),
            "WeightCV": to_float(chars.get("WeightCV", 0.0)),
            "UniqueNice": to_int(chars.get("UniqueNice", 0)),
        }
    )

write_rows(ANALYSIS_DIR / "fairness_vs_characteristics.csv", fairness_vs_chars)


with (ANALYSIS_DIR / "dataset_summary.txt").open("w", encoding="utf-8") as f:
    f.write(f"Structured metrics files: {len(metrics_paths)}\n")
    f.write(f"Structured task files: {len(task_paths)}\n")
    f.write(f"Aggregate rows: {len(metrics_rows)}\n")
    f.write(f"Task rows: {len(task_rows)}\n")
    f.write(f"Quality check rows: {len(quality_rows)}\n")
    f.write("Generated canonical files:\n")
    f.write("- metrics_enriched.csv\n")
    f.write("- run_index.csv\n")
    f.write("- quality_checks.csv\n")
    f.write("Generated supplemental files:\n")
    f.write("- weighted_allocation_summary.csv\n")
    f.write("- nice_bucket_allocation.csv\n")
    f.write("- workload_characteristics.csv\n")
    f.write("- fairness_vs_characteristics.csv\n")

print(f"Wrote analysis outputs to: {ANALYSIS_DIR}")
