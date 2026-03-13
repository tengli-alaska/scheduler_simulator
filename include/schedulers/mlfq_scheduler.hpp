/**
 * @file mlfq_scheduler.hpp
 * @brief Multi-Level Feedback Queue scheduler
 */

#pragma once

#include "scheduler/scheduler.hpp"
#include <deque>  // Changed from vector
#include <array>

namespace sched_sim {

class MLFQScheduler : public Scheduler {
public:
    static constexpr int NUM_QUEUES = 3;
    static constexpr double BOOST_INTERVAL = 100.0;
    
    MLFQScheduler();
    
    void submit_task(TaskPtr task) override;
    TaskPtr select_next(int core_id) override;
    void task_tick(TaskPtr task, double elapsed, int core_id) override;
    void task_completed(TaskPtr task) override;
    
    std::string name() const override { return "MLFQ"; }
    
    uint32_t promotions() const noexcept { return promotions_; }
    uint32_t demotions() const noexcept { return demotions_; }
    uint32_t boosts() const noexcept { return boosts_; }
    
private:
    void priority_boost();
    
    std::array<std::deque<TaskPtr>, NUM_QUEUES> queues_;  // Changed to deque
    std::array<double, NUM_QUEUES> time_quantums_;
    double last_boost_time_ = 0.0;
    
    uint32_t promotions_ = 0;
    uint32_t demotions_ = 0;
    uint32_t boosts_ = 0;
};

} // namespace sched_sim