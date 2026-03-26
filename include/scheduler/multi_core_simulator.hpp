/**
 * @file multi_core_simulator.hpp
 * @brief Multi-queue multi-server simulator (Option 3)
 *
 * Each core has its own scheduler instance and run queue.
 * A load balancer distributes arriving tasks across cores.
 * Optional work stealing lets idle cores pull from busy queues.
 */

#pragma once

#include "task.hpp"
#include "scheduler.hpp"
#include "event.hpp"
#include "load_balancer.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace sched_sim {

/**
 * @class MultiCoreSimulator
 * @brief Discrete-event simulator with per-core scheduler queues
 */
class MultiCoreSimulator {
public:
    using SchedulerFactory = std::function<SchedulerPtr()>;

    /**
     * @brief Construct multi-queue multi-server simulator
     * @param scheduler_factory Creates a fresh scheduler for each core
     * @param num_cores Number of CPU cores
     * @param balancer Load balancing strategy
     * @param enable_work_stealing Allow idle cores to steal tasks
     * @param default_time_slice Fallback time slice (ms)
     */
    MultiCoreSimulator(SchedulerFactory scheduler_factory,
                       int num_cores,
                       LoadBalancerPtr balancer,
                       bool enable_work_stealing = true,
                       double default_time_slice = 5.0);

    void add_task(TaskPtr task);
    void run(double stop_time);

    // Getters
    const std::vector<TaskPtr>& completed_tasks() const noexcept {
        return completed_tasks_;
    }
    double current_time() const noexcept { return current_time_; }
    int num_cores() const noexcept { return num_cores_; }

    // Statistics
    uint32_t context_switches() const noexcept { return context_switches_; }
    uint32_t preemptions() const noexcept { return preemptions_; }
    uint32_t work_steals() const noexcept { return work_steals_; }

    std::string balancer_name() const { return balancer_->name(); }

private:
    void handle_arrival(const Event& evt);
    void handle_time_slice(const Event& evt);
    void preempt_core(int core_id);
    void dispatch_next(int core_id);
    bool try_work_steal(int idle_core);

    // Per-core schedulers
    std::vector<SchedulerPtr> schedulers_;
    LoadBalancerPtr balancer_;

    EventQueue events_;
    int num_cores_;
    bool enable_work_stealing_;
    double default_time_slice_;
    double current_time_ = 0.0;
    double stop_time_ = 0.0;

    std::vector<TaskPtr> running_tasks_;
    std::vector<double> last_event_time_;
    std::vector<TaskPtr> all_tasks_;
    std::vector<TaskPtr> completed_tasks_;

    uint32_t context_switches_ = 0;
    uint32_t preemptions_ = 0;
    uint32_t work_steals_ = 0;
};

} // namespace sched_sim
