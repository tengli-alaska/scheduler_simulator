#pragma once

#include "single_queue_scheduler.h"

// Simple Round-Robin scheduler as a baseline / test scheduler.
// Uses FIFO ordering with a fixed time quantum.
class RRScheduler : public SingleQueueScheduler {
public:
    explicit RRScheduler(int64_t quantum_us = 10000) // default 10ms quantum
        : SingleQueueScheduler([](const Process* a, const Process* b) {
              // FIFO: earlier entry time first, then lower PID
              if (a->enter_ready_time_us != b->enter_ready_time_us)
                  return a->enter_ready_time_us < b->enter_ready_time_us;
              return a->pid < b->pid;
          }),
          quantum_us_(quantum_us) {}

    std::string name() const override {
        return "Round-Robin (quantum=" + std::to_string(quantum_us_ / 1000) + "ms)";
    }

    ScheduleDecision schedule(int64_t current_time_us) override {
        Process* best = find_best();
        if (!best) return {-1, 0, false};
        return {best->pid, quantum_us_, false};
    }

    void on_cpu_used(Process* proc, int64_t cpu_time_us, int64_t current_time_us) override {
        // No special accounting for RR
    }

    bool should_preempt(Process* new_proc, Process* current_proc,
                        int64_t current_time_us) override {
        // RR doesn't preempt on arrival
        return false;
    }

private:
    int64_t quantum_us_;
};
