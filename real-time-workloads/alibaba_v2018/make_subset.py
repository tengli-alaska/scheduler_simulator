#!/usr/bin/env python3
"""
Create a very small subset from Alibaba Cluster Trace v2018 batch_instance.csv.

Designed for huge files (100GB+): streams input line-by-line and uses
reservoir sampling for uniform random subsets without loading the full file.

Examples:
  python3 make_subset.py --size 200
  python3 make_subset.py --size 500 --mode random --seed 42
  python3 make_subset.py --size 300 --mode head
"""

from __future__ import annotations

import argparse
import csv
import os
import random
import sys
from typing import List, Optional


def looks_like_header(row: List[str]) -> bool:
    # Heuristic: header rows usually contain alphabetic chars in at least one cell.
    return any(any(ch.isalpha() for ch in cell) for cell in row)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a tiny subset from huge batch_instance.csv"
    )
    parser.add_argument(
        "--input",
        default="batch_instance.csv",
        help="Path to source CSV (default: batch_instance.csv)",
    )
    parser.add_argument(
        "--output",
        default="batch_instance_subset.csv",
        help="Path to output CSV (default: batch_instance_subset.csv)",
    )
    parser.add_argument(
        "--size",
        type=int,
        default=500,
        help="Number of data rows in subset (default: 500)",
    )
    parser.add_argument(
        "--mode",
        choices=["random", "head"],
        default="random",
        help="Sampling mode: random (reservoir) or head (first N rows).",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for random mode (default: 42)",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=None,
        help="Optional cap on number of input data rows scanned (for quick tests).",
    )
    parser.add_argument(
        "--no-header",
        action="store_true",
        help="Treat input as having no header row.",
    )
    parser.add_argument(
        "--delimiter",
        default=",",
        help="CSV delimiter (default: ',').",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.size <= 0:
        print("Error: --size must be > 0", file=sys.stderr)
        return 1

    if not os.path.exists(args.input):
        print(f"Error: input file not found: {args.input}", file=sys.stderr)
        return 1

    random.seed(args.seed)

    header: Optional[List[str]] = None
    sampled: List[List[str]] = []
    scanned_rows = 0

    with open(args.input, "r", newline="", encoding="utf-8") as f_in:
        reader = csv.reader(f_in, delimiter=args.delimiter)

        first = next(reader, None)
        if first is None:
            print("Error: input CSV is empty.", file=sys.stderr)
            return 1

        if not args.no_header and looks_like_header(first):
            header = first
        else:
            # First row is data
            if args.mode == "head":
                sampled.append(first)
            else:
                sampled = [first]
            scanned_rows = 1

        for row in reader:
            if args.max_lines is not None and scanned_rows >= args.max_lines:
                break

            scanned_rows += 1

            if args.mode == "head":
                if len(sampled) < args.size:
                    sampled.append(row)
                else:
                    break
            else:
                # Reservoir sampling: uniform sample of size N from stream.
                if len(sampled) < args.size:
                    sampled.append(row)
                else:
                    j = random.randint(1, scanned_rows)
                    if j <= args.size:
                        sampled[j - 1] = row

            if scanned_rows % 1_000_000 == 0:
                print(f"Scanned {scanned_rows:,} rows...", file=sys.stderr)

    with open(args.output, "w", newline="", encoding="utf-8") as f_out:
        writer = csv.writer(f_out, delimiter=args.delimiter)
        if header is not None:
            writer.writerow(header)
        writer.writerows(sampled)

    print(
        f"Done. Wrote {len(sampled):,} rows"
        f"{' + header' if header is not None else ''} to {args.output}"
    )
    print(f"Scanned {scanned_rows:,} data rows from {args.input}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
