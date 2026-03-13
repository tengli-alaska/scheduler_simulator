#pragma once

#include <cstdint>
#include <vector>

// Represents a single task/process in the simulation.
// Follows a CPU-burst / IO-burst cycle model.
struct Process {
    int pid;

    // Arrival time in microseconds
    int64_t arrival_time_us;

    // Burst sequence: alternating CPU and IO bursts
    // For trace-driven: single CPU burst, no IO
    // For synthetic: multiple CPU/IO cycles
    std::vector<int64_t> cpu_bursts_us;
    std::vector<int64_t> io_bursts_us;
    int current_burst_index = 0;

    // Priority / weight
    int nice;           // Linux nice value: -20 to +19
    int weight;         // CFS weight derived from nice
    double cpu_request; // Normalized CPU request (from trace)

    // Scheduling class (0=non-prod, 1=batch, 2=mid, 3=latency-sensitive)
    int scheduling_class = 0;

    // Runtime state
    int64_t remaining_burst_us = 0;   // Remaining time in current CPU burst
    int64_t total_cpu_time_us = 0;    // Total CPU time consumed
    int64_t total_wait_time_us = 0;   // Total time spent waiting in queue

    // Timestamps for metrics
    int64_t enter_ready_time_us = 0;  // When this process last entered the ready queue
    int64_t first_run_time_us = -1;   // First time this process ran on CPU
    int64_t completion_time_us = -1;  // When this process finished all bursts

    // MLFQ state
    int mlfq_queue_level = 0;         // Current queue level (0 = highest priority)
    int64_t mlfq_allotment_remaining_us = 0;

    // CFS/EEVDF state
    double vruntime = 0.0;            // Virtual runtime (CFS)
    double virtual_deadline = 0.0;    // Virtual deadline (EEVDF)
    double eligible_time = 0.0;       // Eligibility time (EEVDF)

    // Stride state
    int64_t pass = 0;                 // Pass value (Stride)
    int64_t stride = 0;              // Stride = LARGE_CONST / tickets

    bool is_completed() const {
        return current_burst_index >= static_cast<int>(cpu_bursts_us.size());
    }

    int64_t current_cpu_burst_us() const {
        if (current_burst_index < static_cast<int>(cpu_bursts_us.size()))
            return cpu_bursts_us[current_burst_index];
        return 0;
    }

    int64_t current_io_burst_us() const {
        if (current_burst_index < static_cast<int>(io_bursts_us.size()))
            return io_bursts_us[current_burst_index];
        return 0;
    }

    // Turnaround time = completion - arrival
    int64_t turnaround_time_us() const {
        if (completion_time_us < 0) return -1;
        return completion_time_us - arrival_time_us;
    }

    // Response time = first run - arrival
    int64_t response_time_us() const {
        if (first_run_time_us < 0) return -1;
        return first_run_time_us - arrival_time_us;
    }
};
