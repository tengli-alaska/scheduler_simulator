#pragma once

#include "scheduler.h"
#include <vector>
#include <deque>
#include <algorithm>
#include <cstdint>

// Base class for multi-queue schedulers (MLFQ).
// Manages multiple priority levels, each with its own FIFO queue.
// Subclasses define the number of levels, time quanta per level,
// demotion/promotion rules, and boost behavior.
class MultiQueueScheduler : public Scheduler {
public:
    struct QueueConfig {
        int64_t time_quantum_us;   // Time slice for this queue level
        int64_t allotment_us;      // Total CPU time before demotion (0 = use quantum)
    };

    explicit MultiQueueScheduler(std::vector<QueueConfig> configs)
        : configs_(std::move(configs)),
          queues_(configs_.size()) {}

    void add_process(Process* proc, int64_t current_time_us) override {
        int level = proc->mlfq_queue_level;
        level = std::max(0, std::min(level, num_levels() - 1));
        proc->mlfq_queue_level = level;
        queues_[level].push_back(proc);
    }

    void remove_process(int pid) override {
        for (auto& queue : queues_) {
            queue.erase(
                std::remove_if(queue.begin(), queue.end(),
                               [pid](const Process* p) { return p->pid == pid; }),
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
    // Find the highest-priority non-empty queue and return front process
    Process* find_best() const {
        for (int level = 0; level < num_levels(); level++) {
            if (!queues_[level].empty()) {
                return queues_[level].front();
            }
        }
        return nullptr;
    }

    // Get the time quantum for a given queue level
    int64_t quantum_for_level(int level) const {
        if (level < 0 || level >= num_levels()) return configs_.back().time_quantum_us;
        return configs_[level].time_quantum_us;
    }

    // Get the allotment for a given queue level
    int64_t allotment_for_level(int level) const {
        if (level < 0 || level >= num_levels()) return configs_.back().allotment_us;
        int64_t allot = configs_[level].allotment_us;
        return (allot > 0) ? allot : configs_[level].time_quantum_us;
    }

    // Demote a process to the next lower queue
    void demote(Process* proc) {
        if (proc->mlfq_queue_level < num_levels() - 1) {
            proc->mlfq_queue_level++;
        }
        proc->mlfq_allotment_remaining_us = allotment_for_level(proc->mlfq_queue_level);
    }

    // Boost all processes to the top queue (anti-starvation)
    void boost_all() {
        // Collect all processes from lower queues
        std::vector<Process*> all_procs;
        for (int level = 1; level < num_levels(); level++) {
            for (auto* p : queues_[level]) {
                p->mlfq_queue_level = 0;
                p->mlfq_allotment_remaining_us = allotment_for_level(0);
                all_procs.push_back(p);
            }
            queues_[level].clear();
        }
        // Add to top queue
        for (auto* p : all_procs) {
            queues_[0].push_back(p);
        }
    }

    // Access queues for subclass operations
    std::vector<std::deque<Process*>>& queues() { return queues_; }
    const std::vector<std::deque<Process*>>& queues() const { return queues_; }
    const std::vector<QueueConfig>& configs() const { return configs_; }

private:
    std::vector<QueueConfig> configs_;
    std::vector<std::deque<Process*>> queues_;
};
