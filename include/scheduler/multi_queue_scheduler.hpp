/**
 * @file multi_queue_scheduler.hpp
 * @brief Base class for multi-queue schedulers (MLFQ)
 *
 * Manages multiple priority levels, each with its own FIFO queue.
 * Subclasses define the number of levels, time quanta per level,
 * demotion/promotion rules, and boost behavior.
 */

#pragma once

#include "scheduler.hpp"
#include <vector>
#include <deque>
#include <algorithm>

namespace sched_sim {

class MultiQueueScheduler : public Scheduler {
public:
    struct QueueConfig {
        double time_quantum;    ///< Time slice for this queue level (ms)
        double allotment;       ///< Total CPU time before demotion (0 = use quantum)
    };

    explicit MultiQueueScheduler(std::vector<QueueConfig> configs)
        : configs_(std::move(configs)),
          queues_(configs_.size()) {}

    void add_task(TaskPtr task, double /*current_time*/) override {
        int level = task->current_queue();
        level = std::max(0, std::min(level, num_levels() - 1));
        task->set_current_queue(level);
        queues_[level].push_back(task);
    }

    void remove_task(uint32_t task_id) override {
        for (auto& queue : queues_) {
            queue.erase(
                std::remove_if(queue.begin(), queue.end(),
                               [task_id](const TaskPtr& t) { return t->id() == task_id; }),
                queue.end()
            );
        }
    }

    int ready_count() const override {
        int count = 0;
        for (const auto& queue : queues_) {
            count += static_cast<int>(queue.size());
        }
        return count;
    }

    void reset() override {
        for (auto& queue : queues_) {
            queue.clear();
        }
    }

    int num_levels() const {
        return static_cast<int>(configs_.size());
    }

protected:
    /// Find the highest-priority non-empty queue and return front task
    TaskPtr find_best() const {
        for (int level = 0; level < num_levels(); level++) {
            if (!queues_[level].empty()) {
                return queues_[level].front();
            }
        }
        return nullptr;
    }

    /// Get the time quantum for a given queue level
    double quantum_for_level(int level) const {
        if (level < 0 || level >= num_levels()) return configs_.back().time_quantum;
        return configs_[level].time_quantum;
    }

    /// Get the allotment for a given queue level
    double allotment_for_level(int level) const {
        if (level < 0 || level >= num_levels()) return configs_.back().allotment;
        double allot = configs_[level].allotment;
        return (allot > 0) ? allot : configs_[level].time_quantum;
    }

    /// Demote a task to the next lower queue
    void demote(TaskPtr task) {
        if (task->current_queue() < num_levels() - 1) {
            task->set_current_queue(task->current_queue() + 1);
        }
    }

    /// Boost all tasks to the top queue (anti-starvation)
    void boost_all() {
        std::vector<TaskPtr> all_tasks;
        for (int level = 1; level < num_levels(); level++) {
            for (auto& t : queues_[level]) {
                t->set_current_queue(0);
                all_tasks.push_back(t);
            }
            queues_[level].clear();
        }
        for (auto& t : all_tasks) {
            queues_[0].push_back(t);
        }
    }

    /// Access queues for subclass operations
    std::vector<std::deque<TaskPtr>>& queues() { return queues_; }
    const std::vector<std::deque<TaskPtr>>& queues() const { return queues_; }
    const std::vector<QueueConfig>& configs() const { return configs_; }

private:
    std::vector<QueueConfig> configs_;
    std::vector<std::deque<TaskPtr>> queues_;
};

} // namespace sched_sim
