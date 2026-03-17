/**
 * @file eevdf_scheduler.hpp
 * @brief Earliest Eligible Virtual Deadline First scheduler
 */

#pragma once

#include "scheduler/scheduler.hpp"
#include <vector>

namespace sched_sim {

/**
 * @class EEVDFScheduler
 * @brief Linux 6.6+ EEVDF scheduler implementation
 * 
 * EEVDF extends CFS with deadline-based selection among eligible tasks.
 */
class EEVDFScheduler : public Scheduler {
public:
    explicit EEVDFScheduler(double base_slice = 3.0);
    
    void submit_task(TaskPtr task) override;
    TaskPtr select_next(int core_id) override;
    void task_tick(TaskPtr task, double elapsed, int core_id) override;
    void task_completed(TaskPtr task) override;
    
    std::string name() const override { return "EEVDF"; }
    
private:
    static double calc_delta_fair(double delta, uint32_t weight) noexcept;
    bool is_eligible(const TaskPtr& task) const noexcept;
    void update_deadline(TaskPtr task);
    
    std::vector<TaskPtr> ready_queue_;
    double min_vruntime_ = 0.0;
    double base_slice_;
};

} // namespace sched_sim