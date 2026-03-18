#pragma once

#include "process.h"
#include <string>
#include <vector>

// Result of a scheduling decision.
struct ScheduleDecision {
    int pid = -1;               // Which process to run (-1 = idle)
    int64_t time_slice_us = 0;  // How long to run before preemption check
    bool preempt_current = false; // Should we preempt the currently running process?
};

// Abstract scheduler interface.
// Both single-queue (CFS, EEVDF, Stride) and multi-queue (MLFQ) implement this.
class Scheduler {
public:
    virtual ~Scheduler() = default;

    // Return the scheduler's name for logging/output
    virtual std::string name() const = 0;

    // Add a process to the ready queue(s).
    // Called when a process arrives or returns from IO.
    virtual void add_process(Process* proc, int64_t current_time_us) = 0;

    // Remove a process from the ready queue(s).
    // Called when a process is dispatched to CPU.
    virtual void remove_process(int pid) = 0;

    // Select the next process to run and determine time slice.
    // Returns {pid=-1} if no process is ready.
    virtual ScheduleDecision schedule(int64_t current_time_us) = 0;

    // Notify the scheduler that a process used `cpu_time_us` of CPU time.
    // Used to update vruntime (CFS), pass values (Stride), allotments (MLFQ), etc.
    virtual void on_cpu_used(Process* proc, int64_t cpu_time_us, int64_t current_time_us) = 0;

    // Check if a newly arriving/unblocking process should preempt the current one.
    // `current_proc` may be nullptr if CPU is idle.
    virtual bool should_preempt(Process* new_proc, Process* current_proc,
                                int64_t current_time_us) = 0;

    // Return how many processes are in the ready queue(s)
    virtual int ready_count() const = 0;

    // Reset scheduler state (for running multiple experiments)
    virtual void reset() = 0;
};
