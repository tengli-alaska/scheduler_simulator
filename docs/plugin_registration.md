# Plugin Registration for Researchers

Plugin-style registration supports scheduler/workload selectors used by:

- `scripts/validate_benchmark_spec.py`
- `python3 -m benchmark.runner.run_suite`

## What plugins do

Plugins register benchmark keys (for suite specs) and map them to simulator CLI tokens:

- Scheduler key -> `scheduler_sim -s <cli_token>`
- Workload key -> `scheduler_sim -w <cli_token>`

This lets researchers add aliases and custom benchmark selectors without editing core runner logic.

## Required plugin interface

A plugin file must expose:

```python
def register(registry) -> None:
    ...
```

Inside `register`, call:

- `registry.register_scheduler(SchedulerPlugin(...))`
- `registry.register_workload(WorkloadPlugin(...))`

See template:

- [`templates/new_workload.py`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/templates/new_workload.py)
- [`templates/new_scheduler.py`](/Users/vidyakalyandurg/Desktop/scheduler_simulator/templates/new_scheduler.py)

## Where plugins are loaded from

Loaded automatically:

- `benchmark/plugins/local/*.py` (except `__init__.py`)

Optionally loaded via CLI:

- `--plugins <file_or_dir>[,<file_or_dir>...]`

Also supported through env var:

- `BENCHMARK_PLUGIN_PATHS`

## Example

File: `benchmark/plugins/local/my_workload.py`

```python
from benchmark.plugins import WorkloadPlugin

def register(registry):
    registry.register_workload(
        WorkloadPlugin(
            key="myworkload",
            cli_token="myworkload",
            display_name="MyWorkload",
            aliases=("my-workload",),
        )
    )
```

Then reference in suite runs:

```json
{ "workload": "myworkload", "cores": 4, "num_tasks": 10000, "stop_time": 500000, "topology": "sq" }
```

Important: the `cli_token` must be accepted by your simulator binary (`apps/main.cpp`).
