from __future__ import annotations

import json
import os
from pathlib import Path


def import_plotting_stack():
    cwd = Path.cwd()
    mplconfigdir = Path(os.environ.setdefault("MPLCONFIGDIR", str((cwd / ".mplconfig").resolve())))
    xdg_cache_home = Path(os.environ.setdefault("XDG_CACHE_HOME", str((cwd / ".cache").resolve())))
    os.environ.setdefault("MPLBACKEND", "Agg")
    mplconfigdir.mkdir(parents=True, exist_ok=True)
    xdg_cache_home.mkdir(parents=True, exist_ok=True)

    try:
        import numpy as np
        import pandas as pd
        import seaborn as sns
        import matplotlib.pyplot as plt
    except ImportError as exc:
        msg = (
            "Missing plotting dependencies.\n"
            "Install with:\n"
            "  python3 -m pip install -r scripts/plot_requirements.txt\n"
            f"Original error: {exc}"
        )
        raise SystemExit(msg) from exc
    return pd, sns, plt, np


def resolve_suite_id(suite: str) -> str:
    suite_value = str(suite).strip()
    if suite_value.lower() == "full":
        return "full"

    suite_path = Path(suite_value)
    if suite_path.exists() and suite_path.is_file():
        with suite_path.open("r", encoding="utf-8") as f:
            doc = json.load(f)
        suite_id = str(doc.get("suite_id", "")).strip()
        if not suite_id:
            raise SystemExit(f"Suite spec missing suite_id: {suite_path}")
        return suite_id

    return suite_value


def read_csv_required(analysis_dir: Path, name: str, pd):
    path = analysis_dir / name
    if not path.exists():
        raise SystemExit(f"Missing required analysis file: {path}")
    return pd.read_csv(path)


def read_csv_optional(analysis_dir: Path, name: str, pd):
    path = analysis_dir / name
    if not path.exists():
        return pd.DataFrame()
    return pd.read_csv(path)


def load_analysis_frames(analysis_dir: Path, pd) -> dict:
    frames = {
        "metrics": read_csv_required(analysis_dir, "metrics_enriched.csv", pd),
        "run_index": read_csv_required(analysis_dir, "run_index.csv", pd),
        "quality_checks": read_csv_optional(analysis_dir, "quality_checks.csv", pd),
    }
    return frames


def _filter_by_suite(df, suite_id: str):
    if df.empty or suite_id == "full" or "SuiteId" not in df.columns:
        return df
    return df[df["SuiteId"] == suite_id].copy()


def filter_frames_by_suite(frames: dict, suite_id: str) -> dict:
    return {name: _filter_by_suite(df, suite_id) for name, df in frames.items()}
