/**
 * @file load_balancer.hpp
 * @brief Load balancing strategies for multi-queue multi-server topology
 *
 * Distributes arriving tasks across per-core scheduler queues.
 */

#pragma once

#include "task.hpp"
#include "scheduler.hpp"
#include <vector>
#include <string>
#include <memory>
#include <limits>

namespace sched_sim {

/**
 * @class LoadBalancer
 * @brief Abstract base for task-to-core assignment strategies
 */
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;
    virtual std::string name() const = 0;

    /**
     * @brief Choose which core should receive this task
     * @param task The arriving task
     * @param schedulers Per-core scheduler instances (read queue sizes, etc.)
     * @param running_tasks Which task is running on each core (nullptr = idle)
     * @return Core index [0, num_cores)
     */
    virtual int assign(const TaskPtr& task,
                       const std::vector<SchedulerPtr>& schedulers,
                       const std::vector<TaskPtr>& running_tasks) = 0;
};

using LoadBalancerPtr = std::unique_ptr<LoadBalancer>;

// ============================================================================
// Round-Robin: cycle through cores in order
// ============================================================================
class RoundRobinBalancer : public LoadBalancer {
public:
    std::string name() const override { return "RoundRobin"; }

    int assign(const TaskPtr& /*task*/,
               const std::vector<SchedulerPtr>& schedulers,
               const std::vector<TaskPtr>& /*running_tasks*/) override {
        int core = next_core_ % static_cast<int>(schedulers.size());
        next_core_ = (next_core_ + 1) % static_cast<int>(schedulers.size());
        return core;
    }

private:
    int next_core_ = 0;
};

// ============================================================================
// Least-Loaded: pick core with fewest queued + running tasks
// ============================================================================
class LeastLoadedBalancer : public LoadBalancer {
public:
    std::string name() const override { return "LeastLoaded"; }

    int assign(const TaskPtr& /*task*/,
               const std::vector<SchedulerPtr>& schedulers,
               const std::vector<TaskPtr>& running_tasks) override {
        int best_core = 0;
        int best_load = std::numeric_limits<int>::max();

        for (size_t i = 0; i < schedulers.size(); ++i) {
            int load = schedulers[i]->ready_count() + (running_tasks[i] ? 1 : 0);
            if (load < best_load) {
                best_load = load;
                best_core = static_cast<int>(i);
            }
        }
        return best_core;
    }
};

} // namespace sched_sim
