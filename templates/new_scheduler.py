"""Template: register a custom scheduler plugin for benchmark runner/validator.

Usage:
1) Copy to benchmark/plugins/local/my_scheduler.py
2) Edit `key`, `cli_token`, and metadata
3) Re-run:
     python3 scripts/validate_benchmark_spec.py --suite benchmark/spec/suites/community_v1.json

Important:
- `cli_token` must be accepted by `scheduler_sim -s <token>`.
- If you add a brand-new scheduler in C++, register that CLI token in apps/main.cpp.
"""

from benchmark.plugins import SchedulerPlugin


def register(registry) -> None:
    registry.register_scheduler(
        SchedulerPlugin(
            key="myscheduler",           # Key used in suite specs/defaults
            cli_token="myscheduler",     # Passed to scheduler_sim via -s
            display_name="MyScheduler",  # Human-friendly name for docs/reports
            description="Short description of scheduling policy.",
            aliases=("my-scheduler", "MyScheduler"),
            is_meta=False,               # True only for selector bundles like 'all'
        )
    )
