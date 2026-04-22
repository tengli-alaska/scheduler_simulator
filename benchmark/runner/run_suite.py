#!/usr/bin/env python3
"""Execute benchmark suites directly from JSON spec."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

from benchmark.plugins import build_plugin_registry


ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_SUITE = ROOT_DIR / "benchmark/spec/suites/community_v1.json"
DEFAULT_BIN = ROOT_DIR / "build/bin/scheduler_sim"
DEFAULT_RUNS_DIR = ROOT_DIR / "runs"


def fail(msg: str) -> None:
    raise ValueError(msg)


def sanitize_id(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_-]+", "-", value.strip())
    sanitized = re.sub(r"-{2,}", "-", sanitized)
    return sanitized.strip("-") or "id"


VALID_TOPOLOGIES = {"sq", "mq"}
VALID_BALANCERS = {"rr", "leastloaded"}


def load_suite(path: Path, registry) -> dict:
    if not path.exists():
        raise SystemExit(f"Suite not found: {path}")
    with path.open("r", encoding="utf-8") as f:
        doc = json.load(f)
    validate_suite(doc, registry)
    return doc


def validate_suite(doc: dict, registry) -> None:
    required = [
        "schema_version",
        "benchmark_version",
        "suite_id",
        "title",
        "description",
        "defaults",
        "experiments",
    ]
    for key in required:
        if key not in doc:
            fail(f"missing suite key: {key}")
    if doc["schema_version"] != "suite.v1":
        fail(f"unsupported schema_version: {doc['schema_version']}")
    if not isinstance(doc["experiments"], list):
        fail("experiments must be a list")
    defaults = doc["defaults"]
    for key in ["scheduler", "replications", "topology", "seed_base"]:
        if key not in defaults:
            fail(f"defaults.{key} is required")
    if int(defaults["replications"]) < 1:
        fail("defaults.replications must be >= 1")
    if int(defaults["seed_base"]) < 1:
        fail("defaults.seed_base must be >= 1")
    if defaults["topology"] not in VALID_TOPOLOGIES:
        fail(f"defaults.topology invalid: {defaults['topology']}")
    if registry.resolve_scheduler(defaults["scheduler"]) is None:
        fail(
            f"defaults.scheduler invalid: {defaults['scheduler']}. "
            f"Known scheduler keys: {registry.scheduler_help_tokens()}"
        )

    exp_ids = {str(exp.get("id", "")).strip() for exp in doc["experiments"]}
    for exp in doc["experiments"]:
        exp_id = str(exp.get("id", "")).strip()
        if not exp_id:
            fail("experiment id must be non-empty")
        runs = exp.get("runs", [])
        if not isinstance(runs, list):
            fail(f"{exp_id}.runs must be a list")
        for idx, run in enumerate(runs):
            if registry.resolve_workload(run.get("workload", "")) is None:
                fail(
                    f"{exp_id}.runs[{idx}].workload invalid: {run.get('workload')}. "
                    f"Known workload keys: {registry.workload_help_tokens()}"
                )
            if int(run.get("cores", 0)) < 1:
                fail(f"{exp_id}.runs[{idx}].cores must be >= 1")
            if int(run.get("num_tasks", 0)) < 1:
                fail(f"{exp_id}.runs[{idx}].num_tasks must be >= 1")
            if float(run.get("stop_time", 0.0)) <= 0.0:
                fail(f"{exp_id}.runs[{idx}].stop_time must be > 0")
            topology = str(run.get("topology", "")).lower()
            if topology not in VALID_TOPOLOGIES:
                fail(f"{exp_id}.runs[{idx}].topology invalid: {topology}")
            if topology == "mq":
                balancer = str(run.get("balancer", "")).lower()
                if balancer not in VALID_BALANCERS:
                    fail(f"{exp_id}.runs[{idx}].balancer invalid: {balancer}")
            if "scheduler" in run and registry.resolve_scheduler(run["scheduler"]) is None:
                fail(
                    f"{exp_id}.runs[{idx}].scheduler invalid: {run['scheduler']}. "
                    f"Known scheduler keys: {registry.scheduler_help_tokens()}"
                )

    for exp in doc["experiments"]:
        parent = exp.get("reuse_from_experiment")
        if parent and parent not in exp_ids:
            fail(f"{exp.get('id')}.reuse_from_experiment references unknown experiment: {parent}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run scheduler benchmark suite from JSON spec.")
    parser.add_argument("--suite", default=str(DEFAULT_SUITE), help="Path to suite JSON")
    parser.add_argument("--bin", default=str(DEFAULT_BIN), help="Path to scheduler_sim binary")
    parser.add_argument("--runs-dir", default=str(DEFAULT_RUNS_DIR), help="Path to runs directory")
    parser.add_argument(
        "--only-experiments",
        default="",
        help="Comma-separated experiment IDs to run (default: all with explicit runs)",
    )
    parser.add_argument("--scheduler", default=None, help="Override suite default scheduler")
    parser.add_argument("--replications", type=int, default=None, help="Override default replications")
    parser.add_argument("--seed-base", type=int, default=None, help="Override default seed base")
    parser.add_argument(
        "--no-dedupe",
        action="store_true",
        help="Run duplicate parameter combinations separately per experiment",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print planned commands only")
    parser.add_argument(
        "--continue-on-error",
        action="store_true",
        help="Continue running remaining jobs if a run fails",
    )
    parser.add_argument(
        "--generate-manifests",
        action="store_true",
        help="Generate manifests after successful execution",
    )
    parser.add_argument(
        "--clean-suite-dir",
        action="store_true",
        help="Delete runs/<suite_id>/ before executing planned cases",
    )
    parser.add_argument(
        "--plugins",
        default=os.environ.get("BENCHMARK_PLUGIN_PATHS", ""),
        help="Optional plugin paths (comma-separated files/dirs) to load before execution.",
    )
    return parser.parse_args()


def pick_experiments(all_experiments: list[dict], only: str) -> list[dict]:
    if not only.strip():
        return all_experiments
    wanted = {x.strip() for x in only.split(",") if x.strip()}
    picked = [exp for exp in all_experiments if exp.get("id") in wanted]
    missing = wanted - {exp.get("id") for exp in picked}
    if missing:
        raise SystemExit(f"Unknown experiment ids: {sorted(missing)}")
    return picked


def run_key(run: dict, workload_cli: str, scheduler_cli: str) -> tuple:
    topology = run["topology"]
    balancer = run.get("balancer", "leastloaded") if topology == "mq" else "na"
    ws = run.get("work_stealing", True) if topology == "mq" else None
    return (
        workload_cli,
        scheduler_cli,
        int(run["cores"]),
        int(run["num_tasks"]),
        float(run["stop_time"]),
        topology,
        balancer,
        ws,
    )


def build_run_plan(
    suite: dict,
    selected_experiments: list[dict],
    dedupe: bool,
    scheduler_key: str,
    replications: int,
    seed_base: int,
    runs_dir: Path,
    registry,
) -> list[dict]:
    plan = []
    dedup_map = {}
    case_index = 0

    for exp in selected_experiments:
        exp_id = exp["id"]
        for exp_run_index, run in enumerate(exp.get("runs", []), start=1):
            scheduler_for_run_key = run.get("scheduler", scheduler_key)
            scheduler_plugin = registry.resolve_scheduler(scheduler_for_run_key)
            if scheduler_plugin is None:
                raise SystemExit(
                    f"Unknown scheduler '{scheduler_for_run_key}' in experiment '{exp_id}'. "
                    f"Known keys: {registry.scheduler_help_tokens()}"
                )
            workload_plugin = registry.resolve_workload(run["workload"])
            if workload_plugin is None:
                raise SystemExit(
                    f"Unknown workload '{run['workload']}' in experiment '{exp_id}'. "
                    f"Known keys: {registry.workload_help_tokens()}"
                )

            key = run_key(
                run,
                workload_cli=workload_plugin.cli_token,
                scheduler_cli=scheduler_plugin.cli_token,
            )
            if dedupe and key in dedup_map:
                dedup_map[key]["experiments"].append(exp_id)
                continue

            workload_cli, scheduler_cli_resolved, cores, num_tasks, stop_time, topology, balancer, ws = key
            case_index += 1
            run_id = sanitize_id(
                f"{suite['suite_id']}-{case_index:03d}-{workload_plugin.key}-{cores}c-n{num_tasks}-t{int(stop_time)}"
                + (f"-{balancer}-ws-{'on' if ws else 'off'}" if topology == "mq" else "-sq")
            )
            entry = {
                "case_index": case_index,
                "experiments": [exp_id],
                "exp_run_index": exp_run_index,
                "workload_key": workload_plugin.key,
                "workload_cli": workload_cli,
                "cores": cores,
                "num_tasks": num_tasks,
                "stop_time": stop_time,
                "topology": topology,
                "balancer": balancer,
                "work_stealing": ws,
                "run_id": run_id,
                "seed": seed_base + (case_index * 1000),
                "scheduler_key": scheduler_plugin.key,
                "scheduler_cli": scheduler_cli_resolved,
                "replications": replications,
            }
            entry["artifact_dir"] = str(runs_dir / sanitize_id(suite["suite_id"]) / run_id)
            plan.append(entry)
            dedup_map[key] = entry
    return plan


def build_command(bin_path: Path, suite: dict, case: dict) -> list[str]:
    cmd = [
        str(bin_path),
        "-s",
        case["scheduler_cli"],
        "-w",
        case["workload_cli"],
        "-c",
        str(case["cores"]),
        "-m",
        case["topology"],
        "-n",
        str(case["num_tasks"]),
        "-t",
        str(int(case["stop_time"])),
        "-r",
        str(case["replications"]),
        "--benchmark-version",
        suite["benchmark_version"],
        "--suite-id",
        suite["suite_id"],
        "--run-id",
        case["run_id"],
        "--seed",
        str(case["seed"]),
        "--artifact-dir",
        case["artifact_dir"],
    ]
    if case["topology"] == "mq":
        cmd.extend(["-b", case["balancer"]])
        if case["work_stealing"] is False:
            cmd.append("--no-steal")
    return cmd


def execute_plan(
    suite: dict,
    plan: list[dict],
    bin_path: Path,
    runs_dir: Path,
    dry_run: bool,
    continue_on_error: bool,
) -> dict:
    logs_dir = runs_dir / "logs" / "suite_runner"
    logs_dir.mkdir(parents=True, exist_ok=True)

    summary = {
        "summary_schema_version": "suite-run.v1",
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "suite_id": suite["suite_id"],
        "benchmark_version": suite["benchmark_version"],
        "binary": str(bin_path),
        "dry_run": dry_run,
        "total_cases": len(plan),
        "cases": [],
    }

    for idx, case in enumerate(plan, start=1):
        cmd = build_command(bin_path, suite, case)
        cmd_display = " ".join(cmd)
        log_path = logs_dir / f"{case['run_id']}.log"
        print(f"[{idx}/{len(plan)}] {cmd_display}")

        case_result = {
            "case_index": case["case_index"],
            "run_id": case["run_id"],
            "experiments": case["experiments"],
            "command": cmd,
            "log_path": str(log_path.relative_to(ROOT_DIR)),
            "status": "planned" if dry_run else "pending",
        }

        if not dry_run:
            with log_path.open("w", encoding="utf-8") as logf:
                proc = subprocess.run(
                    cmd,
                    cwd=ROOT_DIR,
                    stdout=logf,
                    stderr=subprocess.STDOUT,
                    text=True,
                    check=False,
                )
            case_result["return_code"] = proc.returncode
            case_result["status"] = "ok" if proc.returncode == 0 else "failed"
            if proc.returncode != 0 and not continue_on_error:
                summary["cases"].append(case_result)
                summary["stopped_early"] = True
                summary["failed_case_index"] = idx
                summary["failed_run_id"] = case["run_id"]
                break

        summary["cases"].append(case_result)

    ok = sum(1 for c in summary["cases"] if c["status"] == "ok")
    failed = sum(1 for c in summary["cases"] if c["status"] == "failed")
    planned = sum(1 for c in summary["cases"] if c["status"] == "planned")
    summary["ok"] = ok
    summary["failed"] = failed
    summary["planned_only"] = planned
    return summary


def write_summary(summary: dict, suite_id: str, runs_dir: Path) -> Path:
    manifests_dir = runs_dir / "manifests"
    manifests_dir.mkdir(parents=True, exist_ok=True)
    out_path = manifests_dir / f"{sanitize_id(suite_id)}.execution_summary.json"
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, sort_keys=True)
        f.write("\n")
    return out_path


def maybe_generate_manifests(runs_dir: Path) -> None:
    script = ROOT_DIR / "scripts/generate_run_manifests.py"
    if not script.exists():
        print(f"[warn] manifest script not found: {script}")
        return
    subprocess.run(
        ["python3", str(script), "--runs-dir", str(runs_dir)],
        cwd=ROOT_DIR,
        check=False,
    )


def main() -> None:
    args = parse_args()
    suite_path = Path(args.suite).resolve()
    bin_path = Path(args.bin).resolve()
    runs_dir = Path(args.runs_dir).resolve()

    if not bin_path.exists():
        raise SystemExit(f"Simulator binary not found: {bin_path}")

    registry, loaded_plugins = build_plugin_registry(args.plugins)
    suite = load_suite(suite_path, registry)
    selected_experiments = pick_experiments(suite["experiments"], args.only_experiments)
    scheduler = args.scheduler or suite["defaults"]["scheduler"]
    scheduler_plugin = registry.resolve_scheduler(scheduler)
    if scheduler_plugin is None:
        raise SystemExit(
            f"Unknown scheduler '{scheduler}'. "
            f"Known scheduler keys: {registry.scheduler_help_tokens()}"
        )
    replications = args.replications if args.replications is not None else int(suite["defaults"]["replications"])
    seed_base = args.seed_base if args.seed_base is not None else int(suite["defaults"]["seed_base"])

    if replications < 1:
        raise SystemExit("replications must be >= 1")
    if seed_base < 1:
        raise SystemExit("seed-base must be >= 1")

    plan = build_run_plan(
        suite=suite,
        selected_experiments=selected_experiments,
        dedupe=not args.no_dedupe,
        scheduler_key=scheduler_plugin.key,
        replications=replications,
        seed_base=seed_base,
        runs_dir=runs_dir,
        registry=registry,
    )
    if not plan:
        raise SystemExit("No explicit runs found in selected experiments.")

    suite_output_dir = runs_dir / sanitize_id(suite["suite_id"])
    if args.clean_suite_dir and suite_output_dir.exists() and not args.dry_run:
        shutil.rmtree(suite_output_dir)
        print(f"Removed previous suite artifacts: {suite_output_dir}")

    print(
        f"Suite: {suite['suite_id']} ({suite['benchmark_version']})\n"
        f"Experiments selected: {len(selected_experiments)}\n"
        f"Run cases to execute: {len(plan)}\n"
        f"Dedupe: {'off' if args.no_dedupe else 'on'}\n"
        f"Scheduler selector: {scheduler_plugin.key} -> {scheduler_plugin.cli_token}\n"
        f"External plugins loaded: {len(loaded_plugins)}\n"
    )

    summary = execute_plan(
        suite=suite,
        plan=plan,
        bin_path=bin_path,
        runs_dir=runs_dir,
        dry_run=args.dry_run,
        continue_on_error=args.continue_on_error,
    )
    summary_path = write_summary(summary, suite["suite_id"], runs_dir)
    print(f"\nExecution summary: {summary_path}")

    if args.generate_manifests and not args.dry_run:
        maybe_generate_manifests(runs_dir)

    if summary.get("failed", 0) > 0:
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except ValueError as exc:
        print(f"Suite runner error: {exc}", file=sys.stderr)
        raise SystemExit(1)
