# Scheduler Simulator

A C++17 discrete-event simulator for evaluating CPU scheduling policies across single-core and multi-core configurations.

## Features

- **4 Schedulers**: CFS, EEVDF, MLFQ, Stride
- **2 Workloads**: Server (web/API/batch), Desktop (UI/shell/compile/indexing)
- **Multi-core support**: Single queue, multi-server (shared queue across N cores)
- **Metrics**: Response time (mean, P95, P99), turnaround, throughput, Jain's fairness, context switches, preemptions
- **Run tracking**: Results saved to `runs/` with CLI parameters in the filename

## Architecture

```
SingleQueueScheduler (base)          MultiQueueScheduler (base)
    |       |        |                        |
   CFS    EEVDF   Stride                    MLFQ
```

- **SingleQueueScheduler**: Shared base for CFS, EEVDF, Stride with pluggable comparator
- **MultiQueueScheduler**: Shared base for MLFQ with priority queues, demotion, boost

## Build

```bash
make clean && make
```

## Usage

```bash
# Run all schedulers on all workloads (1 core, 100 tasks)
./build/bin/scheduler_sim

# Specify number of tasks and cores
./build/bin/scheduler_sim -n 50 -c 4

# Run a specific scheduler on a specific workload
./build/bin/scheduler_sim -s cfs -w server

# Run EEVDF on desktop with 200 tasks and 4 cores
./build/bin/scheduler_sim -s eevdf -w desktop -n 200 -c 4

# Run with multiple replications
./build/bin/scheduler_sim -n 100 -c 2 -r 5
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-n <num>` | Number of tasks per workload | 100 |
| `-c <num>` | Number of CPU cores | 1 |
| `-r <num>` | Number of replications | 1 |
| `-t <time>` | Simulation stop time (ms) | 10000.0 |
| `-s <name>` | Scheduler: `cfs`, `eevdf`, `mlfq`, `stride`, `all` | all |
| `-w <name>` | Workload: `server`, `desktop`, `all` | all |
| `-h` | Show help | |

## Results

Results are saved to the `runs/` directory with parameters encoded in the filename:

```
runs/n50_c4_s-cfs_w-server_r1.csv
runs/n100_c1_s-all_w-all_r5.csv
```

Each CSV contains:

```
Scheduler, Workload, Completed, MeanRT, P95RT, P99RT, MeanTAT, MeanWT,
Throughput, JainsFairness, ContextSwitches, Preemptions
```

## Project Structure

```
include/
  scheduler/              Core framework
    scheduler.hpp           Scheduler interface (add_task, schedule, should_preempt, ...)
    single_queue_scheduler.hpp  Base for CFS/EEVDF/Stride
    multi_queue_scheduler.hpp   Base for MLFQ
    simulator.hpp           Discrete-event simulator (single queue, multi-core)
    task.hpp                Task definition
    event.hpp               Event queue
    metrics.hpp             Metrics calculation
    workload.hpp            Workload generator interface
  schedulers/             Scheduler implementations (header-only)
    cfs_scheduler.hpp
    eevdf_scheduler.hpp
    mlfq_scheduler.hpp
    stride_scheduler.hpp
src/
  core/                   Core implementation (.cpp)
  workloads/              Workload generators (server, desktop)
apps/
  main.cpp                Experiment runner
lib/                      External C libraries (RNG)
runs/                     Simulation results (gitignored)
```

## Adding a New Scheduler

1. Create `include/schedulers/my_scheduler.hpp`
2. Extend `SingleQueueScheduler` or `MultiQueueScheduler`
3. Implement: `schedule()`, `on_cpu_used()`, `should_preempt()`
4. Add to `apps/main.cpp` scheduler list

## Adding a New Workload

1. Add class to `include/scheduler/workload.hpp` extending `WorkloadGenerator`
2. Create `src/workloads/my_workload.cpp` implementing `generate()`
3. Add to `apps/main.cpp` workload list

## Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- Make

## License

MIT License
