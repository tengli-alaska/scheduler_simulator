from __future__ import annotations

from .registry import BenchmarkPluginRegistry, SchedulerPlugin, WorkloadPlugin


def register_builtin_plugins(registry: BenchmarkPluginRegistry) -> None:
    # Scheduler selectors supported by apps/main.cpp.
    registry.register_scheduler(
        SchedulerPlugin(
            key="cfs",
            cli_token="cfs",
            display_name="CFS",
            description="Linux-like Completely Fair Scheduler.",
            aliases=("CFS",),
        )
    )
    registry.register_scheduler(
        SchedulerPlugin(
            key="eevdf",
            cli_token="eevdf",
            display_name="EEVDF",
            description="Earliest Eligible Virtual Deadline First.",
            aliases=("EEVDF",),
        )
    )
    registry.register_scheduler(
        SchedulerPlugin(
            key="mlfq",
            cli_token="mlfq",
            display_name="MLFQ",
            description="Multi-Level Feedback Queue scheduler.",
            aliases=("MLFQ",),
        )
    )
    registry.register_scheduler(
        SchedulerPlugin(
            key="stride",
            cli_token="stride",
            display_name="Stride",
            description="Deterministic proportional-share scheduler.",
            aliases=("Stride",),
        )
    )
    registry.register_scheduler(
        SchedulerPlugin(
            key="all",
            cli_token="all",
            display_name="All Schedulers",
            description="Meta selector to run all registered schedulers in the simulator binary.",
            is_meta=True,
        )
    )

    # Workload selectors supported by apps/main.cpp.
    registry.register_workload(
        WorkloadPlugin(
            key="server",
            cli_token="server",
            display_name="Server",
            description="Synthetic server-like workload with varied service times.",
            aliases=("Server",),
        )
    )
    registry.register_workload(
        WorkloadPlugin(
            key="desktop",
            cli_token="desktop",
            display_name="Desktop",
            description="Synthetic desktop-like interactive workload.",
            aliases=("Desktop",),
        )
    )
    registry.register_workload(
        WorkloadPlugin(
            key="google",
            cli_token="google",
            display_name="GoogleTraceV3",
            description="Trace-replay workload based on Google cluster trace v3.",
            aliases=("GoogleTraceV3", "googletracev3"),
        )
    )
    registry.register_workload(
        WorkloadPlugin(
            key="alibaba",
            cli_token="alibaba",
            display_name="AlibabaTraceV2018",
            description="Trace-replay workload based on Alibaba 2018 trace.",
            aliases=("AlibabaTraceV2018", "alibabatracev2018"),
        )
    )
    registry.register_workload(
        WorkloadPlugin(
            key="all",
            cli_token="all",
            display_name="All Workloads",
            description="Meta selector to run all registered workloads in the simulator binary.",
            is_meta=True,
        )
    )
