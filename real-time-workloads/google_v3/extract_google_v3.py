#!/usr/bin/env python3
"""
Extract Google Borg V3 (ClusterData2019) instance_events into simulator CSV.

Input:  instance_events JSON shards (gzipped or plain)
Output: CSV with columns:
    arrival_time_us, cpu_burst_duration_us, io_burst_duration_us, nice_value,
    weight, scheduling_class, cpu_request

Pipeline:
  1. Stream JSON lines, collect SUBMIT(0), SCHEDULE(1), FINISH(4) events
  2. Group by task key = (collection_id, instance_index)
  3. For complete tasks (have SUBMIT + SCHEDULE + FINISH):
       arrival_time = SUBMIT.time
       cpu_burst    = FINISH.time - SCHEDULE.time
  4. Map V3 priority tiers to Linux nice values
  5. Output sorted by arrival_time

V3 Priority Tiers -> Nice mapping:
  Free (0-99)           -> nice +19 to +10  (low priority)
  Best-effort (100-115) -> nice +9 to +5    (batch)
  Mid (116-119)         -> nice +4 to +1    (mid tier)
  Production (120-359)  -> nice 0 to -10    (high priority)
  Monitoring (360+)     -> nice -11 to -20  (highest priority)
"""

import sys
import json
import gzip
import os
import argparse
from collections import defaultdict

# V3 event types
SUBMIT   = '0'
SCHEDULE = '1'  # Called QUEUE in V3 docs, functionally = scheduled
FINISH   = '4'
KILL     = '5'
EVICT    = '3'

# Linux CFS weight table (nice -20 to +19)
CFS_WEIGHTS = {
    -20: 88761, -19: 71755, -18: 56483, -17: 46273, -16: 36291,
    -15: 29154, -14: 23254, -13: 18705, -12: 14949, -11: 11916,
    -10:  9548,  -9:  7620,  -8:  6100,  -7:  4904,  -6:  3906,
     -5:  3121,  -4:  2501,  -3:  1991,  -2:  1586,  -1:  1277,
      0:  1024,   1:   820,   2:   655,   3:   526,   4:   423,
      5:   335,   6:   272,   7:   215,   8:   172,   9:   137,
     10:   110,  11:    87,  12:    70,  13:    56,  14:    45,
     15:    36,  16:    29,  17:    23,  18:    18,  19:    15,
}


def map_priority_to_nice(v3_priority: int) -> int:
    """Map Google V3 priority (0-500+) to Linux nice value (-20 to +19).

    V3 tiers (from docs):
      Free:        0-99    -> nice +19 to +10
      Best-effort: 100-115 -> nice +9 to +5
      Mid:         116-119 -> nice +4 to +1
      Production:  120-359 -> nice  0 to -10
      Monitoring:  360+    -> nice -11 to -20
    """
    if v3_priority < 100:
        # Free tier: map 0->19, 99->10
        nice = 19 - int(v3_priority * 9 / 99)
        return max(10, min(19, nice))
    elif v3_priority <= 115:
        # Best-effort batch: map 100->9, 115->5
        nice = 9 - int((v3_priority - 100) * 4 / 15)
        return max(5, min(9, nice))
    elif v3_priority <= 119:
        # Mid tier: map 116->4, 119->1
        nice = 4 - int((v3_priority - 116) * 3 / 3)
        return max(1, min(4, nice))
    elif v3_priority <= 359:
        # Production: map 120->0, 359->-10
        nice = -int((v3_priority - 120) * 10 / 239)
        return max(-10, min(0, nice))
    else:
        # Monitoring: map 360->-11, 500->-20
        nice = -11 - int((v3_priority - 360) * 9 / 140)
        return max(-20, min(-11, nice))


def open_file(path):
    """Open gzipped or plain JSON file."""
    if path.endswith('.gz'):
        return gzip.open(path, 'rt', encoding='utf-8')
    else:
        return open(path, 'r', encoding='utf-8')


def extract_tasks(input_files, max_tasks=None, time_window_ns=None):
    """
    Two-pass extraction:
      Pass 1: Collect all events grouped by task key
      Pass 2: Match SUBMIT->SCHEDULE->FINISH and compute durations

    Args:
        input_files: list of JSON file paths
        max_tasks: optional limit on output tasks
        time_window_ns: optional, only include tasks with SUBMIT time < this value
    """
    # task_key -> {'submit': [], 'schedule': [], 'finish': []}
    tasks = defaultdict(lambda: {'submit': [], 'schedule': [], 'finish': []})

    total_lines = 0
    parse_errors = 0

    for fpath in input_files:
        print(f"Reading {os.path.basename(fpath)}...", file=sys.stderr)
        with open_file(fpath) as f:
            for line in f:
                total_lines += 1
                if total_lines % 5_000_000 == 0:
                    print(f"  {total_lines:,} lines processed, "
                          f"{len(tasks):,} unique tasks seen...", file=sys.stderr)

                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    parse_errors += 1
                    continue

                etype = d.get('type')
                if etype not in (SUBMIT, SCHEDULE, FINISH):
                    continue

                time_ns = int(d.get('time', 0))

                # Apply time window filter on SUBMIT events
                if time_window_ns and etype == SUBMIT and time_ns > time_window_ns:
                    continue

                key = (d.get('collection_id', ''), d.get('instance_index', ''))

                event_data = {
                    'time': time_ns,
                    'priority': int(d.get('priority', 0)),
                    'scheduling_class': int(d.get('scheduling_class', 0)),
                    'cpus': d.get('resource_request', {}).get('cpus', 0.0),
                }

                if etype == SUBMIT:
                    tasks[key]['submit'].append(event_data)
                elif etype == SCHEDULE:
                    tasks[key]['schedule'].append(event_data)
                elif etype == FINISH:
                    tasks[key]['finish'].append(event_data)

    print(f"\nTotal lines: {total_lines:,}, Parse errors: {parse_errors:,}", file=sys.stderr)
    print(f"Unique task keys: {len(tasks):,}", file=sys.stderr)

    # Match events and produce output records
    records = []
    stats = {
        'complete': 0,
        'no_submit': 0,
        'no_schedule': 0,
        'no_finish': 0,
        'negative_burst': 0,
        'zero_burst': 0,
    }

    for key, events in tasks.items():
        if not events['submit']:
            stats['no_submit'] += 1
            continue
        if not events['schedule']:
            stats['no_schedule'] += 1
            continue
        if not events['finish']:
            stats['no_finish'] += 1
            continue

        # Use earliest SUBMIT, earliest SCHEDULE after submit, earliest FINISH after schedule
        submit_ev = min(events['submit'], key=lambda e: e['time'])

        # Find schedule event >= submit time
        valid_schedules = [e for e in events['schedule'] if e['time'] >= submit_ev['time']]
        if not valid_schedules:
            # Fallback: use earliest schedule
            valid_schedules = events['schedule']
        schedule_ev = min(valid_schedules, key=lambda e: e['time'])

        # Find finish event >= schedule time
        valid_finishes = [e for e in events['finish'] if e['time'] >= schedule_ev['time']]
        if not valid_finishes:
            stats['no_finish'] += 1
            continue
        finish_ev = min(valid_finishes, key=lambda e: e['time'])

        # Compute durations (V3 times are in nanoseconds, convert to microseconds)
        arrival_us = submit_ev['time'] // 1000
        cpu_burst_us = (finish_ev['time'] - schedule_ev['time']) // 1000

        if cpu_burst_us < 0:
            stats['negative_burst'] += 1
            continue
        if cpu_burst_us == 0:
            stats['zero_burst'] += 1
            continue

        # Use submit event's priority for mapping
        v3_priority = submit_ev['priority']
        nice = map_priority_to_nice(v3_priority)
        weight = CFS_WEIGHTS[nice]
        sched_class = submit_ev['scheduling_class']
        cpu_request = submit_ev['cpus']

        # No direct I/O data in V3 instance_events; set to 0
        io_burst_us = 0

        records.append((
            arrival_us,
            cpu_burst_us,
            io_burst_us,
            nice,
            weight,
            sched_class,
            cpu_request,
        ))

        stats['complete'] += 1
        if max_tasks and stats['complete'] >= max_tasks:
            break

    print(f"\nExtraction stats:", file=sys.stderr)
    for k, v in stats.items():
        print(f"  {k}: {v:,}", file=sys.stderr)

    # Sort by arrival time
    records.sort(key=lambda r: r[0])
    return records


def normalize_arrival_times(records):
    """Shift arrival times so the trace starts at t=0."""
    if not records:
        return records
    min_time = records[0][0]  # already sorted by arrival
    return [(r[0] - min_time, *r[1:]) for r in records]


def write_csv(records, output_path):
    """Write records to CSV."""
    with open(output_path, 'w') as f:
        f.write("arrival_time_us,cpu_burst_duration_us,io_burst_duration_us,"
                "nice,weight,scheduling_class,cpu_request\n")
        for r in records:
            f.write(f"{r[0]},{r[1]},{r[2]},{r[3]},{r[4]},{r[5]},{r[6]:.6f}\n")


def print_summary(records):
    """Print summary statistics."""
    if not records:
        print("No records to summarize.", file=sys.stderr)
        return

    arrivals = [r[0] for r in records]
    bursts = [r[1] for r in records]
    nices = [r[3] for r in records]
    cpus = [r[6] for r in records]

    print(f"\n{'='*60}", file=sys.stderr)
    print(f"WORKLOAD SUMMARY", file=sys.stderr)
    print(f"{'='*60}", file=sys.stderr)
    print(f"Total tasks:        {len(records):,}", file=sys.stderr)
    print(f"Time span:          {arrivals[-1]/1e6:.1f} seconds", file=sys.stderr)
    print(f"", file=sys.stderr)
    print(f"CPU burst (us):     min={min(bursts):,}  median={sorted(bursts)[len(bursts)//2]:,}  "
          f"max={max(bursts):,}  mean={sum(bursts)/len(bursts):,.0f}", file=sys.stderr)
    print(f"", file=sys.stderr)

    from collections import Counter
    nice_dist = Counter(nices)
    print(f"Nice value distribution:", file=sys.stderr)
    for n in sorted(nice_dist.keys()):
        print(f"  nice {n:+3d} (weight {CFS_WEIGHTS[n]:>5d}): {nice_dist[n]:>8,} tasks "
              f"({nice_dist[n]/len(records)*100:.1f}%)", file=sys.stderr)

    print(f"\nCPU request:        min={min(cpus):.4f}  max={max(cpus):.4f}  "
          f"mean={sum(cpus)/len(cpus):.4f}", file=sys.stderr)
    print(f"{'='*60}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Extract Google Borg V3 traces into simulator CSV format"
    )
    parser.add_argument('input_files', nargs='+',
                        help='Instance events JSON files (gzipped or plain)')
    parser.add_argument('-o', '--output', default='google_v3_workload.csv',
                        help='Output CSV path (default: google_v3_workload.csv)')
    parser.add_argument('-n', '--max-tasks', type=int, default=None,
                        help='Maximum number of complete tasks to extract')
    parser.add_argument('-t', '--time-window', type=float, default=None,
                        help='Only include tasks submitted within first T seconds')
    parser.add_argument('--no-normalize', action='store_true',
                        help='Do not shift arrival times to start at 0')

    args = parser.parse_args()

    # Convert time window from seconds to nanoseconds
    time_window_ns = None
    if args.time_window:
        time_window_ns = int(args.time_window * 1e9)

    # Validate input files
    for f in args.input_files:
        if not os.path.exists(f):
            print(f"Error: {f} not found", file=sys.stderr)
            sys.exit(1)

    print(f"Extracting from {len(args.input_files)} file(s)...", file=sys.stderr)
    if args.max_tasks:
        print(f"Limiting to {args.max_tasks:,} tasks", file=sys.stderr)
    if args.time_window:
        print(f"Time window: first {args.time_window:.0f} seconds", file=sys.stderr)

    records = extract_tasks(args.input_files, args.max_tasks, time_window_ns)

    if not args.no_normalize:
        records = normalize_arrival_times(records)

    print_summary(records)

    write_csv(records, args.output)
    print(f"\nWrote {len(records):,} tasks to {args.output}", file=sys.stderr)


if __name__ == '__main__':
    main()
