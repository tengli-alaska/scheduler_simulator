# Scheduler Simulator

A C++17 discrete-event simulator for evaluating CPU scheduling policies across single-core and multi-core configurations, including real-world trace replay.

## Features

- **4 Schedulers**: CFS, EEVDF, MLFQ, Stride
- **4 Workloads**: Server, Desktop, Google Borg V3 trace, Alibaba Cluster V2018 trace
- **3 Topologies**: Single-queue single-server (SQSS), Single-queue multi-server (SQMS), Multi-queue multi-server (MQMS)
- **Load balancers** (MQMS): Round-Robin, Least-Loaded
- **Work stealing**: Idle cores steal from busiest queue (MQMS only)
- **Metrics**: Response time (mean, P95, P99), turnaround, throughput, Jain's fairness, context switches, preemptions
- **Run tracking**: Results saved to `runs/` with CLI parameters encoded in the filename

## Architecture

```
Topology Options
├── SQSS  Single-queue single-server   (-c 1, default)
├── SQMS  Single-queue multi-server    (-c N, -m sq)
└── MQMS  Multi-queue multi-server     (-c N, -m mq)

Scheduler Hierarchy
├── SingleQueueScheduler (base)
│     ├── CFS
│     ├── EEVDF
│     └── Stride
└── MultiQueueScheduler (base)
      └── MLFQ

Workloads
├── ServerWorkload       Synthetic: bursty requests, API calls, background jobs
├── DesktopWorkload      Synthetic: UI events, shell commands, compilations
├── TraceReplayWorkload  GoogleV3 — Google Borg V3 cluster trace (CSV)
└── TraceReplayWorkload  AlibabaV2018 — Alibaba Cluster Trace v2018 (CSV)
```

## Build

```bash
make clean && make
```

Or with CMake:

```bash
cmake -B cmake-build -S . && cmake --build cmake-build
```

## Usage

```bash
# Run all schedulers on all workloads (1 core, 100 tasks)
./cmake-build/bin/scheduler_sim

# Specify tasks and cores (SQMS: shared queue, 4 cores)
./cmake-build/bin/scheduler_sim -n 500 -c 4

# MQMS with least-loaded balancer and work stealing
./cmake-build/bin/scheduler_sim -n 500 -c 4 -m mq

# MQMS with round-robin balancer
./cmake-build/bin/scheduler_sim -n 500 -c 4 -m mq -b rr

# MQMS without work stealing
./cmake-build/bin/scheduler_sim -n 500 -c 4 -m mq --no-steal

# Run a specific scheduler and workload
./cmake-build/bin/scheduler_sim -s cfs -w server

# Run with Google trace workload
./cmake-build/bin/scheduler_sim -w google -n 1000 -c 4

# Run with Alibaba trace workload
./cmake-build/bin/scheduler_sim -w alibaba -n 1000 -c 4

# Multiple replications
./cmake-build/bin/scheduler_sim -n 100 -c 4 -r 5
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-n <num>` | Number of tasks per workload | 100 |
| `-c <num>` | Number of CPU cores | 1 |
| `-r <num>` | Number of replications | 1 |
| `-t <time>` | Simulation stop time (ms) | 10000.0 |
| `-s <name>` | Scheduler: `cfs`, `eevdf`, `mlfq`, `stride`, `all` | all |
| `-w <name>` | Workload: `server`, `desktop`, `google`, `alibaba`, `all` | all |
| `-m <name>` | Topology: `sq` (single-queue), `mq` (multi-queue) | sq |
| `-b <name>` | Load balancer (mq only): `rr`, `leastloaded` | leastloaded |
| `--no-steal` | Disable work stealing (mq only) | — |
| `-h` | Show help | — |

## Testing Configurations

The following 6 configurations are used to evaluate each scheduler:

| # | Topology | Cores | Tasks | Command |
|---|----------|-------|-------|---------|
| 1 | SQSS | 1 | 10,000 | `-c 1 -m sq -n 10000` |
| 2 | SQSS | 1 | 100,000 | `-c 1 -m sq -n 100000` |
| 3 | SQMS | 4 | 10,000 | `-c 4 -m sq -n 10000` |
| 4 | SQMS | 4 | 100,000 | `-c 4 -m sq -n 100000` |
| 5 | MQMS (RR) | 4 | 10,000 | `-c 4 -m mq -b rr -n 10000` |
| 6 | MQMS (RR) | 4 | 100,000 | `-c 4 -m mq -b rr -n 100000` |

Run all 6 for a single scheduler (e.g. CFS):

```bash
./cmake-build/bin/scheduler_sim -s cfs -c 1 -m sq -n 10000
./cmake-build/bin/scheduler_sim -s cfs -c 1 -m sq -n 100000
./cmake-build/bin/scheduler_sim -s cfs -c 4 -m sq -n 10000
./cmake-build/bin/scheduler_sim -s cfs -c 4 -m sq -n 100000
./cmake-build/bin/scheduler_sim -s cfs -c 4 -m mq -b rr -n 10000
./cmake-build/bin/scheduler_sim -s cfs -c 4 -m mq -b rr -n 100000
```

## Real-World Trace Workloads

### Google Borg V3
Place the extracted CSV at:
```
real-time-workloads/google_v3/google_v3_workload.csv
```
Expected columns: `arrival_time_us`, `cpu_burst_duration_us`, `nice`

Use the extraction script:
```bash
python3 real-time-workloads/google_v3/extract_google_v3.py
```

### Alibaba Cluster Trace V2018
Place the subset CSV at:
```
real-time-workloads/alibaba_v2018/batch_instance_subset_head_40000_with_header.csv
```
Expected columns: `task_type`, `status`, `start_time`, `end_time`

Use the subset script:
```bash
python3 real-time-workloads/alibaba_v2018/make_subset.py
```

Both traces are normalized to `t=0` at simulation start.

## Results

Results are saved to `runs/` with parameters encoded in the filename:

```
runs/n100_c4_s-all_w-all_r1_m-sq.csv
runs/n10000_c4_s-cfs_w-server_r1_m-mq_b-rr_steal-on.csv
```

Each CSV row contains:

```
Scheduler, Workload, Completed, MeanRT, P95RT, P99RT, MeanTAT, MeanWT,
Throughput, JainsFairness, ContextSwitches, Preemptions
```

## Project Structure

```
include/
  scheduler/              Core framework
    scheduler.hpp           Scheduler interface
    single_queue_scheduler.hpp  Base for CFS/EEVDF/Stride
    multi_queue_scheduler.hpp   Base for MLFQ
    simulator.hpp           SQSS/SQMS discrete-event simulator
    multi_core_simulator.hpp    MQMS per-core simulator
    load_balancer.hpp       RoundRobin and LeastLoaded balancers
    task.hpp                Task definition
    event.hpp               Event queue
    metrics.hpp             Metrics calculation
    workload.hpp            Workload interface + TraceReplayWorkload
  schedulers/             Scheduler implementations (header-only)
    cfs_scheduler.hpp
    eevdf_scheduler.hpp
    mlfq_scheduler.hpp
    stride_scheduler.hpp
src/
  core/                   Core implementation (.cpp)
  workloads/              Workload generators
    server_workload.cpp
    desktop_workload.cpp
    trace_replay_workload.cpp
apps/
  main.cpp                Experiment runner
real-time-workloads/
  google_v3/              Google Borg V3 extraction scripts + CSV (gitignored)
  alibaba_v2018/          Alibaba V2018 subset scripts + CSV (gitignored)
lib/                      External C libraries (RNG)
runs/                     Simulation results (gitignored)
```

## Adding a New Scheduler

1. Create `include/schedulers/my_scheduler.hpp`
2. Extend `SingleQueueScheduler` or `MultiQueueScheduler`
3. Implement: `schedule()`, `on_cpu_used()`, `should_preempt()`
4. Register in `apps/main.cpp` scheduler list

## Adding a New Workload

1. Add class to `include/scheduler/workload.hpp` extending `WorkloadGenerator`
2. Create `src/workloads/my_workload.cpp` implementing `generate()`
3. Register in `apps/main.cpp` workload list

## Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- Make or CMake 3.14+

## License

MIT License
