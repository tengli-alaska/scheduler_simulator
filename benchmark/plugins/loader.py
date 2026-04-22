from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path

from .builtins import register_builtin_plugins
from .registry import BenchmarkPluginRegistry


ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_LOCAL_PLUGIN_DIR = ROOT_DIR / "benchmark" / "plugins" / "local"


def _split_plugin_paths(raw: str) -> list[str]:
    if not raw.strip():
        return []
    # Support comma and OS path-separator.
    values: list[str] = []
    for chunk in raw.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        if os.pathsep in chunk:
            for part in chunk.split(os.pathsep):
                part = part.strip()
                if part:
                    values.append(part)
        else:
            values.append(chunk)
    return values


def _discover_python_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_file():
            if path.suffix == ".py" and path.name != "__init__.py":
                files.append(path.resolve())
            continue
        if path.is_dir():
            for item in sorted(path.glob("*.py")):
                if item.name == "__init__.py":
                    continue
                files.append(item.resolve())
    seen: set[str] = set()
    unique: list[Path] = []
    for f in files:
        s = str(f)
        if s in seen:
            continue
        seen.add(s)
        unique.append(f)
    return unique


def _load_plugin_module(path: Path):
    mod_name = f"benchmark_user_plugin_{path.stem}_{abs(hash(str(path)))}"
    spec = importlib.util.spec_from_file_location(mod_name, str(path))
    if spec is None or spec.loader is None:
        raise ValueError(f"Unable to load plugin module spec: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[mod_name] = module
    spec.loader.exec_module(module)
    return module


def _register_external_plugins(registry: BenchmarkPluginRegistry, plugin_paths: list[Path]) -> list[str]:
    loaded: list[str] = []
    for file_path in _discover_python_files(plugin_paths):
        module = _load_plugin_module(file_path)
        register_fn = getattr(module, "register", None)
        if not callable(register_fn):
            raise ValueError(
                f"Plugin file missing required callable register(registry): {file_path}"
            )
        register_fn(registry)
        loaded.append(str(file_path.relative_to(ROOT_DIR)) if file_path.is_relative_to(ROOT_DIR) else str(file_path))
    return loaded


def build_plugin_registry(plugin_paths_raw: str = "") -> tuple[BenchmarkPluginRegistry, list[str]]:
    registry = BenchmarkPluginRegistry()
    register_builtin_plugins(registry)

    path_values = _split_plugin_paths(plugin_paths_raw)
    paths = [Path(p).resolve() for p in path_values]
    if DEFAULT_LOCAL_PLUGIN_DIR.exists():
        paths.append(DEFAULT_LOCAL_PLUGIN_DIR.resolve())

    loaded = _register_external_plugins(registry, paths)
    return registry, loaded
