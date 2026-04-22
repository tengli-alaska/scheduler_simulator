"""Template: register a custom workload plugin for benchmark runner/validator.

Usage:
1) Copy to benchmark/plugins/local/my_workload.py
2) Edit `key`, `cli_token`, and metadata
3) Re-run:
     python3 scripts/validate_benchmark_spec.py --suite benchmark/spec/suites/community_v1.json

Important:
- `cli_token` must be accepted by `scheduler_sim -w <token>`.
- If you add a brand-new workload generator in C++, register that CLI token in apps/main.cpp.
"""

from benchmark.plugins import WorkloadPlugin


def register(registry) -> None:
    registry.register_workload(
        WorkloadPlugin(
            key="myworkload",           # Key used in suite specs
            cli_token="myworkload",     # Passed to scheduler_sim via -w
            display_name="MyWorkload",  # Human-friendly name for docs/reports
            description="Short description of workload characteristics.",
            aliases=("my-workload", "MyWorkload"),
            is_meta=False,              # True only for selector-style bundles (e.g., 'all')
        )
    )
