#!/usr/bin/env python3
"""Validate benchmark suite JSON files (phase-1 lightweight validator).

This validator intentionally uses only Python stdlib so contributors do not
need additional dependencies for basic suite checks.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from benchmark.plugins import build_plugin_registry


VALID_TOPOLOGIES = {"sq", "mq"}
VALID_BALANCERS = {"rr", "leastloaded"}


def fail(msg: str) -> None:
    raise ValueError(msg)


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as exc:
        fail(f"{path}: invalid JSON: {exc}")


def validate_top_level(doc: dict) -> None:
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
            fail(f"missing required top-level key: {key}")

    if doc["schema_version"] != "suite.v1":
        fail(f"unsupported schema_version: {doc['schema_version']} (expected suite.v1)")
    if not isinstance(doc["benchmark_version"], str) or not doc["benchmark_version"].strip():
        fail("benchmark_version must be a non-empty string")
    if not isinstance(doc["suite_id"], str) or not doc["suite_id"].strip():
        fail("suite_id must be a non-empty string")
    if not isinstance(doc["experiments"], list) or not doc["experiments"]:
        fail("experiments must be a non-empty list")


def validate_defaults(defaults: dict, registry) -> None:
    for key in ["scheduler", "replications", "topology", "seed_base"]:
        if key not in defaults:
            fail(f"defaults.{key} is required")
    if registry.resolve_scheduler(defaults["scheduler"]) is None:
        fail(
            "defaults.scheduler must be registered plugin key or alias. "
            f"Got '{defaults['scheduler']}'. "
            f"Known scheduler keys: {registry.scheduler_help_tokens()}"
        )
    if int(defaults["replications"]) < 1:
        fail("defaults.replications must be >= 1")
    if defaults["topology"] not in VALID_TOPOLOGIES:
        fail(f"defaults.topology must be one of {sorted(VALID_TOPOLOGIES)}")
    if int(defaults["seed_base"]) < 1:
        fail("defaults.seed_base must be >= 1")


def validate_run(run: dict, exp_id: str, idx: int, registry) -> None:
    run_required = ["workload", "cores", "num_tasks", "stop_time", "topology"]
    for key in run_required:
        if key not in run:
            fail(f"{exp_id}.runs[{idx}] missing required key: {key}")

    if registry.resolve_workload(run["workload"]) is None:
        fail(
            f"{exp_id}.runs[{idx}].workload invalid: '{run['workload']}'. "
            f"Known workload keys: {registry.workload_help_tokens()}"
        )
    if int(run["cores"]) < 1:
        fail(f"{exp_id}.runs[{idx}].cores must be >= 1")
    if int(run["num_tasks"]) < 1:
        fail(f"{exp_id}.runs[{idx}].num_tasks must be >= 1")
    if float(run["stop_time"]) <= 0:
        fail(f"{exp_id}.runs[{idx}].stop_time must be > 0")
    if run["topology"] not in VALID_TOPOLOGIES:
        fail(f"{exp_id}.runs[{idx}].topology invalid: {run['topology']}")

    if run["topology"] == "mq":
        if "balancer" not in run:
            fail(f"{exp_id}.runs[{idx}] mq run requires balancer")
        if run["balancer"] not in VALID_BALANCERS:
            fail(f"{exp_id}.runs[{idx}].balancer invalid: {run['balancer']}")
        if "work_stealing" in run and not isinstance(run["work_stealing"], bool):
            fail(f"{exp_id}.runs[{idx}].work_stealing must be boolean")

    if "scheduler" in run and registry.resolve_scheduler(run["scheduler"]) is None:
        fail(
            f"{exp_id}.runs[{idx}].scheduler invalid: '{run['scheduler']}'. "
            f"Known scheduler keys: {registry.scheduler_help_tokens()}"
        )

def validate_experiments(experiments: list[dict], registry) -> None:
    seen = set()
    for exp in experiments:
        for key in ["id", "title", "objective", "runs"]:
            if key not in exp:
                fail(f"experiment missing required key: {key}")
        exp_id = exp["id"]
        if exp_id in seen:
            fail(f"duplicate experiment id: {exp_id}")
        seen.add(exp_id)
        if not isinstance(exp["runs"], list):
            fail(f"{exp_id}.runs must be a list")
        for idx, run in enumerate(exp["runs"]):
            validate_run(run, exp_id, idx, registry)

    for exp in experiments:
        parent = exp.get("reuse_from_experiment")
        if parent and parent not in seen:
            fail(f"{exp['id']}.reuse_from_experiment references unknown experiment: {parent}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate benchmark suite spec JSON.")
    parser.add_argument(
        "--suite",
        default="benchmark/spec/suites/community_v1.json",
        help="Path to suite JSON file",
    )
    parser.add_argument(
        "--plugins",
        default="",
        help="Optional plugin paths (comma-separated files/dirs) to load before validation.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    suite_path = Path(args.suite).resolve()
    if not suite_path.exists():
        raise SystemExit(f"Suite not found: {suite_path}")

    registry, loaded_plugins = build_plugin_registry(args.plugins)
    doc = load_json(suite_path)
    validate_top_level(doc)
    validate_defaults(doc["defaults"], registry)
    validate_experiments(doc["experiments"], registry)

    exp_count = len(doc["experiments"])
    run_count = sum(len(exp["runs"]) for exp in doc["experiments"])
    print(
        f"Valid suite: {suite_path}\n"
        f"  schema_version={doc['schema_version']}\n"
        f"  benchmark_version={doc['benchmark_version']}\n"
        f"  suite_id={doc['suite_id']}\n"
        f"  experiments={exp_count}\n"
        f"  explicit_runs={run_count}\n"
        f"  scheduler_plugins={len(registry.scheduler_keys())}\n"
        f"  workload_plugins={len(registry.workload_keys())}\n"
        f"  external_plugins_loaded={len(loaded_plugins)}"
    )


if __name__ == "__main__":
    try:
        main()
    except ValueError as exc:
        print(f"Spec validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
