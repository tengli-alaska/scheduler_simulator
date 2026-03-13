/**
 * @file scheduler.hpp
 * @brief Abstract base class for all scheduling policies
 */

#pragma once

#include "task.hpp"
#include <string>
#include <memory>

namespace sched_sim {

// Forward declaration
class Simulator;

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
    
    /**
     * @brief Initialize scheduler with simulator reference
     * @param sim Pointer to the simulator instance
     */
    virtual void initialize(Simulator* sim) { simulator_ = sim; }
    
    /**
     * @brief Submit a new task to the scheduler
     * @param task Shared pointer to the task
     */
    virtual void submit_task(TaskPtr task) = 0;
    
    /**
     * @brief Select the next task to run on given core
     * @param core_id ID of the CPU core requesting a task
     * @return Shared pointer to selected task, or nullptr if none available
     */
    virtual TaskPtr select_next(int core_id) = 0;
    
    /**
     * @brief Called when a task's time slice expires
     * @param task The task that was running
     * @param elapsed Time elapsed since task started running
     * @param core_id ID of the core the task was running on
     */
    virtual void task_tick(TaskPtr task, double elapsed, int core_id) = 0;
    
    /**
     * @brief Called when a task completes execution
     * @param task The completed task
     */
    virtual void task_completed(TaskPtr task) = 0;
    
    /**
     * @brief Get the name of the scheduling algorithm
     * @return String name of the scheduler
     */
    virtual std::string name() const = 0;
    
    // Statistics
    uint32_t context_switches() const noexcept { return context_switches_; }
    uint32_t preemptions() const noexcept { return preemptions_; }
    
protected:
    Simulator* simulator_ = nullptr;
    uint32_t context_switches_ = 0;
    uint32_t preemptions_ = 0;
};

using SchedulerPtr = std::unique_ptr<Scheduler>;

} // namespace sched_sim