/**
 * @file scheduler.hpp
 * @brief Abstract base class for all scheduling policies
 *
 * Common interface that both single-queue (CFS, EEVDF, Stride) and
 * multi-queue (MLFQ) schedulers implement. The simulator drives scheduling
 * through this interface.
 */

#pragma once

#include "task.hpp"
#include <string>
#include <memory>

namespace sched_sim {

/**
 * @struct ScheduleDecision
 * @brief Result of a scheduling decision
 */
struct ScheduleDecision {
    TaskPtr task = nullptr;      ///< Which task to run (nullptr = idle)
    double time_slice = 0.0;     ///< How long to run before preemption check (ms)
};

/**
 * @class Scheduler
 * @brief Abstract base class for scheduling policies
 *
 * All scheduling algorithms must inherit from this class and implement
 * the pure virtual functions.
 */
class Scheduler {
public:
    virtual ~Scheduler() = default;

    /// Return the scheduler's name for logging/output
    virtual std::string name() const = 0;

    /// Add a task to the ready queue(s).
    /// Called when a task arrives or is preempted back into the queue.
    virtual void add_task(TaskPtr task, double current_time) = 0;

    /// Remove a task from the ready queue(s).
    /// Called when a task is dispatched to CPU.
    virtual void remove_task(uint32_t task_id) = 0;

    /// Select the next task to run and determine time slice.
    /// Returns a decision with task=nullptr if no task is ready.
    virtual ScheduleDecision schedule(double current_time) = 0;

    /// Notify the scheduler that a task used cpu_time of CPU time.
    /// Used to update vruntime (CFS), pass values (Stride), allotments (MLFQ), etc.
    virtual void on_cpu_used(TaskPtr task, double cpu_time, double current_time) = 0;

    /// Check if a newly arriving task should preempt the current one.
    /// current_task may be nullptr if CPU is idle.
    virtual bool should_preempt(TaskPtr new_task, TaskPtr current_task,
                                double current_time) = 0;

    /// Return how many tasks are in the ready queue(s)
    virtual int ready_count() const = 0;

    /// Reset scheduler state (for running multiple experiments)
    virtual void reset() = 0;
};

using SchedulerPtr = std::unique_ptr<Scheduler>;

} // namespace sched_sim