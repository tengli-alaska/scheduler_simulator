from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class SchedulerPlugin:
    key: str
    cli_token: str
    display_name: str
    description: str = ""
    aliases: tuple[str, ...] = field(default_factory=tuple)
    is_meta: bool = False


@dataclass(frozen=True)
class WorkloadPlugin:
    key: str
    cli_token: str
    display_name: str
    description: str = ""
    aliases: tuple[str, ...] = field(default_factory=tuple)
    is_meta: bool = False


def _norm(value: str) -> str:
    return str(value).strip().lower()


class BenchmarkPluginRegistry:
    def __init__(self) -> None:
        self._schedulers: dict[str, SchedulerPlugin] = {}
        self._workloads: dict[str, WorkloadPlugin] = {}
        self._scheduler_alias_to_key: dict[str, str] = {}
        self._workload_alias_to_key: dict[str, str] = {}

    def register_scheduler(self, plugin: SchedulerPlugin) -> None:
        key = _norm(plugin.key)
        if not key:
            raise ValueError("scheduler plugin key must be non-empty")
        if key in self._schedulers:
            raise ValueError(f"duplicate scheduler plugin key: {key}")

        normalized = SchedulerPlugin(
            key=key,
            cli_token=_norm(plugin.cli_token),
            display_name=plugin.display_name.strip() or plugin.key,
            description=plugin.description.strip(),
            aliases=tuple(sorted({_norm(a) for a in plugin.aliases if _norm(a)})),
            is_meta=bool(plugin.is_meta),
        )
        if not normalized.cli_token:
            raise ValueError(f"scheduler plugin '{key}' has empty cli_token")

        self._schedulers[key] = normalized
        self._scheduler_alias_to_key[key] = key
        for alias in normalized.aliases:
            if alias == key:
                continue
            if alias in self._scheduler_alias_to_key:
                raise ValueError(
                    f"scheduler alias conflict: '{alias}' already maps to "
                    f"'{self._scheduler_alias_to_key[alias]}'"
                )
            self._scheduler_alias_to_key[alias] = key

    def register_workload(self, plugin: WorkloadPlugin) -> None:
        key = _norm(plugin.key)
        if not key:
            raise ValueError("workload plugin key must be non-empty")
        if key in self._workloads:
            raise ValueError(f"duplicate workload plugin key: {key}")

        normalized = WorkloadPlugin(
            key=key,
            cli_token=_norm(plugin.cli_token),
            display_name=plugin.display_name.strip() or plugin.key,
            description=plugin.description.strip(),
            aliases=tuple(sorted({_norm(a) for a in plugin.aliases if _norm(a)})),
            is_meta=bool(plugin.is_meta),
        )
        if not normalized.cli_token:
            raise ValueError(f"workload plugin '{key}' has empty cli_token")

        self._workloads[key] = normalized
        self._workload_alias_to_key[key] = key
        for alias in normalized.aliases:
            if alias == key:
                continue
            if alias in self._workload_alias_to_key:
                raise ValueError(
                    f"workload alias conflict: '{alias}' already maps to "
                    f"'{self._workload_alias_to_key[alias]}'"
                )
            self._workload_alias_to_key[alias] = key

    def resolve_scheduler(self, value: str) -> SchedulerPlugin | None:
        key = self._scheduler_alias_to_key.get(_norm(value))
        if not key:
            return None
        return self._schedulers.get(key)

    def resolve_workload(self, value: str) -> WorkloadPlugin | None:
        key = self._workload_alias_to_key.get(_norm(value))
        if not key:
            return None
        return self._workloads.get(key)

    def scheduler_keys(self) -> list[str]:
        return sorted(self._schedulers.keys())

    def workload_keys(self) -> list[str]:
        return sorted(self._workloads.keys())

    def scheduler_help_tokens(self) -> list[str]:
        return sorted({p.key for p in self._schedulers.values()})

    def workload_help_tokens(self) -> list[str]:
        return sorted({p.key for p in self._workloads.values()})
