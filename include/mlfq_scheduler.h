#pragma once

#include "multi_queue_scheduler.h"
#include <algorithm>
#include <cstdint>

// Multi-Level Feedback Queue (MLFQ) Scheduler
//
// Key idea: learn process behavior over time by observing CPU usage patterns.
// Interactive processes (short CPU bursts, frequent IO) stay in high-priority
// queues; CPU-bound processes that exhaust their allotment get demoted to
// lower-priority queues with longer time quanta.
//
// Rules (OSTEP formulation):
//   1. If Priority(A) > Priority(B), A runs (B doesn't)
//   2. If Priority(A) == Priority(B), A & B run in round-robin within queue
//   3. When a job enters the system, it starts at the highest priority
//   4. Once a job uses up its allotment at a given level (regardless of how
//      many times it gave up the CPU), its priority is reduced (demoted)
//   5. After some time period S, move all jobs to the topmost queue (boost)
//
// Rule 4 uses allotment tracking (not per-quantum accounting) to prevent
// gaming — a process can't avoid demotion by releasing the CPU just before
// the quantum expires.
//
// Default configuration: 3 queue levels
//   Queue 0 (highest): quantum = 10ms, allotment = 20ms
//   Queue 1 (middle):  quantum = 20ms, allotment = 40ms
//   Queue 2 (lowest):  quantum = 40ms, allotment = infinite (never demote)
//
// Priority boost period S = 100ms (anti-starvation)
//
// Reference: Corbató et al. (1962), Arpaci-Dusseau (OSTEP Ch. 8)
class MLFQScheduler : public MultiQueueScheduler {
public:
    struct Params {
        int num_queues;
        std::vector<QueueConfig> queue_configs;
        int64_t boost_period_us;
        bool enable_boost;
        Params() : num_queues(3),
                   queue_configs({
                       {10000,  20000},  // Queue 0: 10ms quantum, 20ms allotment
                       {20000,  40000},  // Queue 1: 20ms quantum, 40ms allotment
                       {40000,      0},  // Queue 2: 40ms quantum, no demotion (bottom)
                   }),
                   boost_period_us(100000),  // 100ms priority boost period (S)
                   enable_boost(true) {}
    };

    explicit MLFQScheduler(Params params = Params())
        : MultiQueueScheduler(params.queue_configs),
          params_(params),
          time_since_last_boost_us_(0) {}

    std::string name() const override {
        return "MLFQ (" + std::to_string(num_levels()) + " levels, boost=" +
               std::to_string(params_.boost_period_us / 1000) + "ms)";
    }

    void add_process(Process* proc, int64_t current_time_us) override {
        if (proc->first_run_time_us < 0) {
            // Rule 3: new processes start at highest priority
            proc->mlfq_queue_level = 0;
            proc->mlfq_allotment_remaining_us = allotment_for_level(0);
        }
        // Returning processes keep their current queue level
        MultiQueueScheduler::add_process(proc, current_time_us);
    }

    ScheduleDecision schedule(int64_t /*current_time_us*/) override {
        // Rule 1 & 2: find highest-priority non-empty queue, take front (RR)
        Process* best = find_best();
        if (!best) return {-1, 0, false};

        int level = best->mlfq_queue_level;
        int64_t quantum = quantum_for_level(level);

        return {best->pid, quantum, false};
    }

    void on_cpu_used(Process* proc, int64_t cpu_time_us,
                     int64_t current_time_us) override {
        // Rule 4: track allotment (total CPU time at this level)
        proc->mlfq_allotment_remaining_us -= cpu_time_us;

        if (proc->mlfq_allotment_remaining_us <= 0) {
            // Allotment exhausted: demote to next lower queue
            if (proc->mlfq_queue_level < num_levels() - 1) {
                proc->mlfq_queue_level++;
                proc->mlfq_allotment_remaining_us =
                    allotment_for_level(proc->mlfq_queue_level);
            } else {
                // Already at bottom queue: reset allotment (stays here)
                proc->mlfq_allotment_remaining_us =
                    allotment_for_level(proc->mlfq_queue_level);
            }
        }

        // Rule 5: periodic priority boost
        if (params_.enable_boost) {
            time_since_last_boost_us_ += cpu_time_us;
            if (time_since_last_boost_us_ >= params_.boost_period_us) {
                perform_boost(current_time_us);
                time_since_last_boost_us_ = 0;
            }
        }
    }

    bool should_preempt(Process* new_proc, Process* current_proc,
                        int64_t /*current_time_us*/) override {
        if (!current_proc) return true;

        // Preempt if the new process is in a higher-priority queue
        return new_proc->mlfq_queue_level < current_proc->mlfq_queue_level;
    }

    void reset() override {
        MultiQueueScheduler::reset();
        time_since_last_boost_us_ = 0;
    }

private:
    void perform_boost(int64_t /*current_time_us*/) {
        // Rule 5: move all processes to the topmost queue
        boost_all();
        // Also reset allotments
        for (auto& queue : queues()) {
            for (auto* p : queue) {
                p->mlfq_allotment_remaining_us = allotment_for_level(0);
            }
        }
    }

    Params params_;
    int64_t time_since_last_boost_us_;
};
