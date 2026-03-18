/**
 * @file simulator.hpp
 * @brief Main discrete-event simulator
 */

#pragma once

#include "task.hpp"
#include "scheduler.hpp"
#include "event.hpp"
#include <vector>
#include <memory>

namespace sched_sim {

/**
 * @class Simulator
 * @brief Main simulation engine for scheduler evaluation
 */
class Simulator {
public:
    /**
     * @brief Construct a new Simulator
     * @param scheduler Unique pointer to scheduler instance
     * @param num_cores Number of CPU cores to simulate
     * @param default_time_slice Fallback time slice if scheduler returns 0 (ms)
     */
    Simulator(SchedulerPtr scheduler,
              int num_cores = 1,
              double default_time_slice = 5.0);

    void add_task(TaskPtr task);
    void run(double stop_time);

    // Getters
    const std::vector<TaskPtr>& completed_tasks() const noexcept {
        return completed_tasks_;
    }

    double current_time() const noexcept { return current_time_; }
    int num_cores() const noexcept { return num_cores_; }

    const Scheduler* scheduler() const noexcept { return scheduler_.get(); }

    // Statistics
    uint32_t context_switches() const noexcept { return context_switches_; }
    uint32_t preemptions() const noexcept { return preemptions_; }

private:
    void handle_arrival(const Event& evt);
    void handle_time_slice(const Event& evt);
    void preempt_core(int core_id);
    void dispatch_next(int core_id);

    SchedulerPtr scheduler_;
    EventQueue events_;

    int num_cores_;
    double default_time_slice_;
    double current_time_ = 0.0;
    double stop_time_ = 0.0;

    std::vector<TaskPtr> running_tasks_;
    std::vector<double> last_event_time_;
    std::vector<TaskPtr> all_tasks_;
    std::vector<TaskPtr> completed_tasks_;

    uint32_t context_switches_ = 0;
    uint32_t preemptions_ = 0;
};

} // namespace sched_sim
