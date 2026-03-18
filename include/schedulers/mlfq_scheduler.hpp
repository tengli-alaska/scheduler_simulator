/**
 * @file mlfq_scheduler.hpp
 * @brief Multi-Level Feedback Queue scheduler
 *
 * Learns process behavior over time by observing CPU usage patterns.
 * Interactive tasks (short CPU bursts) stay in high-priority queues;
 * CPU-bound tasks that exhaust their allotment get demoted.
 *
 * Rules (OSTEP formulation):
 *   1. If Priority(A) > Priority(B), A runs
 *   2. If Priority(A) == Priority(B), round-robin within queue
 *   3. New jobs start at highest priority
 *   4. Once allotment exhausted at a level, demote
 *   5. Periodic priority boost (anti-starvation)
 *
 * Reference: Corbato et al. (1962), Arpaci-Dusseau (OSTEP Ch. 8)
 */

#pragma once

#include "scheduler/multi_queue_scheduler.hpp"
#include <algorithm>

namespace sched_sim {

class MLFQScheduler : public MultiQueueScheduler {
public:
    struct Params {
        std::vector<QueueConfig> queue_configs;
        double boost_interval;    // Priority boost period (ms)
        bool enable_boost;
        Params() : queue_configs({
                       {2.0, 4.0},    // Queue 0 (highest): 2ms quantum, 4ms allotment
                       {4.0, 8.0},    // Queue 1 (medium):  4ms quantum, 8ms allotment
                       {8.0, 0.0},    // Queue 2 (lowest):  8ms quantum, no demotion
                   }),
                   boost_interval(100.0),
                   enable_boost(true) {}
    };

    explicit MLFQScheduler(Params params = Params())
        : MultiQueueScheduler(params.queue_configs),
          params_(params),
          time_since_last_boost_(0.0) {}

    std::string name() const override { return "MLFQ"; }

    void add_task(TaskPtr task, double current_time) override {
        if (task->start_time() < 0) {
            // Rule 3: new tasks start at highest priority
            task->set_current_queue(0);
            task->set_allotment_remaining(allotment_for_level(0));
        }
        // Returning tasks keep their current queue level
        MultiQueueScheduler::add_task(task, current_time);
    }

    ScheduleDecision schedule(double /*current_time*/) override {
        // Rule 1 & 2: find highest-priority non-empty queue, take front (RR)
        TaskPtr best = find_best();
        if (!best) return {nullptr, 0.0};

        int level = best->current_queue();
        double quantum = quantum_for_level(level);

        return {best, quantum};
    }

    void on_cpu_used(TaskPtr task, double cpu_time, double current_time) override {
        // Rule 4: track allotment
        task->set_allotment_remaining(task->allotment_remaining() - cpu_time);

        if (task->allotment_remaining() <= 0) {
            // Allotment exhausted: demote
            if (task->current_queue() < num_levels() - 1) {
                task->set_current_queue(task->current_queue() + 1);
            }
            task->set_allotment_remaining(allotment_for_level(task->current_queue()));
        }

        // Rule 5: periodic priority boost
        if (params_.enable_boost) {
            time_since_last_boost_ += cpu_time;
            if (time_since_last_boost_ >= params_.boost_interval) {
                perform_boost();
                time_since_last_boost_ = 0.0;
            }
        }
    }

    bool should_preempt(TaskPtr new_task, TaskPtr current_task,
                        double /*current_time*/) override {
        if (!current_task) return true;
        // Preempt if the new task is in a higher-priority queue
        return new_task->current_queue() < current_task->current_queue();
    }

    void reset() override {
        MultiQueueScheduler::reset();
        time_since_last_boost_ = 0.0;
    }

private:
    void perform_boost() {
        boost_all();
        for (auto& queue : queues()) {
            for (auto& t : queue) {
                t->set_allotment_remaining(allotment_for_level(0));
            }
        }
    }

    Params params_;
    double time_since_last_boost_;
};

} // namespace sched_sim
