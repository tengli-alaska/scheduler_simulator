/**
 * @file stride_scheduler.hpp
 * @brief Stride scheduling - deterministic proportional share
 */

#pragma once

#include "scheduler/scheduler.hpp"
#include <vector>

namespace sched_sim {

/**
 * @class StrideScheduler
 * @brief Stride scheduling for proportional CPU sharing
 * 
 * Each task has a stride (inversely proportional to weight).
 * Select task with minimum pass value, then increment by stride.
 */
class StrideScheduler : public Scheduler {
public:
    static constexpr uint32_t STRIDE_CONSTANT = 10000;
    
    StrideScheduler() = default;
    
    void submit_task(TaskPtr task) override;
    TaskPtr select_next(int core_id) override;
    void task_tick(TaskPtr task, double elapsed, int core_id) override;
    void task_completed(TaskPtr task) override;
    
    std::string name() const override { return "Stride"; }
    
private:
    std::vector<TaskPtr> ready_queue_;
};

} // namespace sched_sim