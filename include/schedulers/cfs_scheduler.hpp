/**
 * @file cfs_scheduler.hpp
 * @brief Completely Fair Scheduler (CFS) implementation
 */

#pragma once

#include "scheduler/scheduler.hpp"
#include <set>

namespace sched_sim {

/**
 * @class CFSScheduler
 * @brief Linux CFS scheduler implementation
 * 
 * CFS uses virtual runtime (vruntime) to provide fair CPU time distribution.
 * Tasks with lower vruntime get selected first.
 */
class CFSScheduler : public Scheduler {
public:
    /**
     * @brief Construct a new CFSScheduler
     * @param min_granularity Minimum time slice (ms)
     * @param latency Target scheduling latency (ms)
     */
    explicit CFSScheduler(double min_granularity = 3.0, double latency = 24.0);
    
    void submit_task(TaskPtr task) override;
    TaskPtr select_next(int core_id) override;
    void task_tick(TaskPtr task, double elapsed, int core_id) override;
    void task_completed(TaskPtr task) override;
    
    std::string name() const override { return "CFS"; }
    
private:
    static double calc_delta_fair(double delta, uint32_t weight) noexcept;
    
    // Comparator for red-black tree (std::set) sorted by vruntime
    struct CompareVruntime {
        bool operator()(const TaskPtr& a, const TaskPtr& b) const noexcept {
            if (a->vruntime() != b->vruntime()) {
                return a->vruntime() < b->vruntime();
            }
            return a->id() < b->id();  // Tie-breaker
        }
    };
    
    std::set<TaskPtr, CompareVruntime> ready_queue_;
    double min_vruntime_ = 0.0;
    double min_granularity_;
    double latency_;
};

} // namespace sched_sim