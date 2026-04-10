#!/usr/bin/env python3
import csv
import glob
import math
import os
import statistics
from collections import defaultdict


ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNS_DIR = os.path.join(ROOT_DIR, "runs")
ANALYSIS_DIR = os.path.join(ROOT_DIR, "analysis")

os.makedirs(ANALYSIS_DIR, exist_ok=True)


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


def read_csv_rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


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


def tag_experiments(row):
    workload = row.get("Workload")
    n = row.get("NumTasks")
    c = row.get("Cores")
    t = row.get("StopTime")
    top = row.get("Topology")
    bal = row.get("Balancer")
    ws = row.get("WorkStealing")

    exp1 = (
        workload in {"Server", "Desktop"}
        and top == "sq"
        and n == 10000
        and c in {1, 2, 4}
        and approx_eq(t, 100000.0)
    )

    exp2 = (
        workload in {"Server", "Desktop"}
        and top == "sq"
        and c == 4
        and (
            (n == 10000 and approx_eq(t, 100000.0))
            or (n == 50000 and approx_eq(t, 1000000.0))
            or (n == 100000 and approx_eq(t, 1000000.0))
        )
    )

    # Accept server stop_time 10000 OR 100000 here to handle either command variant.
    exp3 = (
        (workload == "Desktop" and top == "sq" and c == 4 and n == 10000 and approx_eq(t, 100000.0))
        or (workload == "Server" and top == "sq" and c == 4 and n == 10000 and (approx_eq(t, 10000.0) or approx_eq(t, 100000.0)))
        or (workload == "GoogleTraceV3" and top == "sq" and c == 4 and n == 1000 and approx_eq(t, 1000000.0))
        or (workload == "AlibabaTraceV2018" and top == "sq" and c == 4 and n == 1000 and approx_eq(t, 1000000.0))
    )

    exp5 = (
        workload == "Desktop"
        and c == 4
        and n == 10000
        and approx_eq(t, 100000.0)
        and (
            (top == "sq")
            or (top == "mq" and bal == "rr" and ws in {"on", "off"})
            or (top == "mq" and bal == "leastloaded" and ws in {"on", "off"})
        )
    )

    exp4 = exp3
    exp6 = exp3

    tag_names = []
    if exp1:
        tag_names.append("Exp1")
    if exp2:
        tag_names.append("Exp2")
    if exp3:
        tag_names.append("Exp3")
    if exp4:
        tag_names.append("Exp4")
    if exp5:
        tag_names.append("Exp5")
    if exp6:
        tag_names.append("Exp6")

    return {
        "Exp1": int(exp1),
        "Exp2": int(exp2),
        "Exp3": int(exp3),
        "Exp4": int(exp4),
        "Exp5": int(exp5),
        "Exp6": int(exp6),
        "ExperimentList": ";".join(tag_names) if tag_names else "none",
    }


metrics_paths = sorted(
    p for p in glob.glob(os.path.join(RUNS_DIR, "*.csv")) if not p.endswith("_tasks.csv")
)
task_paths = sorted(glob.glob(os.path.join(RUNS_DIR, "*_tasks.csv")))

if not metrics_paths:
    raise SystemExit("No aggregate CSV files found in runs/. Run scripts/run_matrix.sh first.")

metrics_rows = []
for path in metrics_paths:
    for row in read_csv_rows(path):
        row["SourceFile"] = os.path.basename(path)
        row["Replication"] = to_int(row.get("Replication", 0))
        row["NumTasks"] = to_int(row.get("NumTasks", 0))
        row["Cores"] = to_int(row.get("Cores", 0))
        row["StopTime"] = to_float(row.get("StopTime", 0))
        row["Completed"] = to_int(row.get("Completed", 0))
        row["CompletionRatio"] = to_float(row.get("CompletionRatio", 0))
        row["MeanRT"] = to_float(row.get("MeanRT", 0))
        row["P95RT"] = to_float(row.get("P95RT", 0))
        row["P99RT"] = to_float(row.get("P99RT", 0))
        row["MeanTAT"] = to_float(row.get("MeanTAT", 0))
        row["MeanWT"] = to_float(row.get("MeanWT", 0))
        row["Throughput"] = to_float(row.get("Throughput", 0))
        row["ThroughputPerCore"] = to_float(row.get("ThroughputPerCore", 0))
        row["Utilization"] = to_float(row.get("Utilization", 0))
        row["JainsFairness"] = to_float(row.get("JainsFairness", 0))
        row["ContextSwitches"] = to_int(row.get("ContextSwitches", 0))
        row["Preemptions"] = to_int(row.get("Preemptions", 0))
        row["SchedulerClass"] = (
            "fairness_oriented"
            if row.get("Scheduler") in {"CFS", "EEVDF", "Stride"}
            else "alternative"
        )
        row["WorkloadType"] = (
            "synthetic"
            if row.get("Workload") in {"Server", "Desktop"}
            else "real_trace"
        )
        row["ContextSwitchesPerTask"] = safe_div(row["ContextSwitches"], row["Completed"])
        row["PreemptionsPerTask"] = safe_div(row["Preemptions"], row["Completed"])
        row["TopologyVariant"] = topology_variant(row)
        row.update(tag_experiments(row))
        metrics_rows.append(row)


metrics_out_fields = [
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
    "Exp1",
    "Exp2",
    "Exp3",
    "Exp4",
    "Exp5",
    "Exp6",
    "ExperimentList",
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

with open(os.path.join(ANALYSIS_DIR, "metrics_enriched.csv"), "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=metrics_out_fields)
    writer.writeheader()
    writer.writerows(metrics_rows)


task_rows = []
for path in task_paths:
    for row in read_csv_rows(path):
        row["SourceFile"] = os.path.basename(path)
        row["Replication"] = to_int(row.get("Replication", 0))
        row["NumTasks"] = to_int(row.get("NumTasks", 0))
        row["Cores"] = to_int(row.get("Cores", 0))
        row["StopTime"] = to_float(row.get("StopTime", 0))
        row["TaskID"] = to_int(row.get("TaskID", 0))
        row["Nice"] = to_int(row.get("Nice", 0))
        row["Weight"] = to_float(row.get("Weight", 0))
        row["Arrival"] = to_float(row.get("Arrival", 0))
        row["Execution"] = to_float(row.get("Execution", 0))
        row["AllocatedCPU"] = to_float(row.get("AllocatedCPU", 0))
        row["Remaining"] = to_float(row.get("Remaining", 0))
        row["Started"] = to_int(row.get("Started", 0))
        row["Completed"] = to_int(row.get("Completed", 0))
        row["StartTime"] = to_float(row.get("StartTime", 0))
        row["CompletionTime"] = to_float(row.get("CompletionTime", 0))
        row["ResponseTime"] = to_float(row.get("ResponseTime", 0))
        row["TurnaroundTime"] = to_float(row.get("TurnaroundTime", 0))
        row["WaitTime"] = to_float(row.get("WaitTime", 0))
        row["PreemptionCount"] = to_int(row.get("PreemptionCount", 0))
        task_rows.append(row)


run_key_fields = [
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
    replication, num_tasks, cores, topology, balancer, work_stealing, stop_time, scheduler, workload = key

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

    allocation_rows.append(
        {
            "Replication": replication,
            "NumTasks": num_tasks,
            "Cores": cores,
            "Topology": topology,
            "Balancer": balancer,
            "WorkStealing": work_stealing,
            "StopTime": stop_time,
            "Scheduler": scheduler,
            "Workload": workload,
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
                "Replication": replication,
                "NumTasks": num_tasks,
                "Cores": cores,
                "Topology": topology,
                "Balancer": balancer,
                "WorkStealing": work_stealing,
                "StopTime": stop_time,
                "Scheduler": scheduler,
                "Workload": workload,
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
            "Replication": replication,
            "NumTasks": num_tasks,
            "Cores": cores,
            "Topology": topology,
            "Balancer": balancer,
            "WorkStealing": work_stealing,
            "StopTime": stop_time,
            "Scheduler": scheduler,
            "Workload": workload,
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


def write_rows(path, rows):
    if not rows:
        return
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


write_rows(os.path.join(ANALYSIS_DIR, "weighted_allocation_summary.csv"), allocation_rows)
write_rows(os.path.join(ANALYSIS_DIR, "nice_bucket_allocation.csv"), nice_bucket_rows)
write_rows(os.path.join(ANALYSIS_DIR, "workload_characteristics.csv"), workload_char_rows)


baseline_map = {}
for row in metrics_rows:
    if row["Scheduler"] != "MLFQ":
        continue
    key = (
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

write_rows(os.path.join(ANALYSIS_DIR, "fairness_vs_characteristics.csv"), fairness_vs_chars)


with open(os.path.join(ANALYSIS_DIR, "dataset_summary.txt"), "w") as f:
    f.write(f"Aggregate run files: {len(metrics_paths)}\n")
    f.write(f"Task run files: {len(task_paths)}\n")
    f.write(f"Aggregate rows: {len(metrics_rows)}\n")
    f.write(f"Task rows: {len(task_rows)}\n")
    f.write("Generated files:\n")
    f.write("- metrics_enriched.csv\n")
    f.write("- weighted_allocation_summary.csv\n")
    f.write("- nice_bucket_allocation.csv\n")
    f.write("- workload_characteristics.csv\n")
    f.write("- fairness_vs_characteristics.csv\n")

print(f"Wrote analysis outputs to: {ANALYSIS_DIR}")
