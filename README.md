# Scheduler Simulator

A comprehensive C++17 framework for evaluating CPU scheduling policies.

## Features

- ✅ **4 Schedulers**: CFS, EEVDF, MLFQ, Stride
- ✅ **5 Workloads**: CPU-Bound, I/O-Bound, Mixed, Bursty, Bimodal
- ✅ **Comprehensive Metrics**: Response time, throughput, fairness
- ✅ **Modern C++17**: Clean, type-safe, efficient
- ✅ **Modular Design**: Easy to extend

## Quick Start
```bash
# Build and run
./build.sh --clean --run

# Or manually
mkdir build && cd build
cmake ..
make -j$(nproc)
./bin/scheduler_sim
```

## Usage
```bash
# Basic run (100 tasks, 1 core)
./bin/scheduler_sim

# Custom configuration
./bin/scheduler_sim -n 200 -c 2 -r 5

# Options:
#   -n <num>   Number of tasks (default: 100)
#   -c <num>   Number of cores (default: 1)
#   -r <num>   Replications (default: 1)
#   -h         Help
```

## Results

Output saved to `results.csv`:
- Mean/Median/P95/P99 response time
- Throughput and utilization
- Fairness metrics
- Context switches and preemptions

## Project Structure
```
include/           Public headers
src/              Implementation
lib/              External C libraries (RNG)
apps/             Main application
```

## Adding Components

### New Scheduler

1. Create `include/schedulers/my_scheduler.hpp`
2. Inherit from `Scheduler` base class
3. Implement virtual methods
4. Add to CMakeLists.txt and main.cpp

### New Workload

1. Create class inheriting `WorkloadGenerator`
2. Implement `generate()` method
3. Add to main.cpp

## Requirements

- CMake 3.15+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)

## License

MIT License