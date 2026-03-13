#pragma once

#include "single_queue_scheduler.h"
#include <algorithm>
#include <cstdint>
#include <limits>

// Stride Scheduling
//
// Key idea: deterministic proportional-share scheduling. Each process has
// a "ticket" count (derived from its weight). The stride is inversely
// proportional to tickets: stride = STRIDE_CONSTANT / tickets.
// Each time a process runs, its "pass" counter advances by stride.
// The process with the lowest pass value runs next.
//
// This gives exactly proportional CPU shares over time:
//   share_i = tickets_i / total_tickets = weight_i / total_weight
//
// Unlike lottery scheduling (randomized), Stride is deterministic and
// gives perfectly proportional shares with minimal variance.
//
// Time quantum is fixed (Waldspurger default: 10ms). All processes get
// the same physical time per scheduling round; the pass/stride mechanism
// ensures higher-ticket processes get scheduled more frequently.
//
// Reference: Waldspurger & Weihl (1995), "Stride Scheduling"
class StrideScheduler : public SingleQueueScheduler {
public:
    struct Params {
        int64_t quantum_us;
        int64_t stride_constant;
        Params() : quantum_us(10000), stride_constant(1000000) {}
    };

    explicit StrideScheduler(Params params = Params())
        : SingleQueueScheduler(stride_comparator),
          params_(params),
          global_pass_(0) {}

    std::string name() const override {
        return "Stride (quantum=" + std::to_string(params_.quantum_us / 1000) + "ms)";
    }

    void add_process(Process* proc, int64_t /*current_time_us*/) override {
        // Compute stride from weight (weight acts as ticket count)
        // Higher weight = more tickets = smaller stride = runs more often
        proc->stride = params_.stride_constant / std::max(proc->weight, 1);

        if (proc->first_run_time_us < 0) {
            // New process: set pass to global_pass to avoid starvation of
            // existing processes and prevent unfair burst of CPU time
            proc->pass = global_pass_;
        } else {
            // Returning from IO/preemption: clamp pass to not fall too far behind
            // This prevents a process from hogging CPU after sleeping
            proc->pass = std::max(proc->pass, global_pass_);
        }

        SingleQueueScheduler::add_process(proc, 0);
    }

    ScheduleDecision schedule(int64_t /*current_time_us*/) override {
        Process* best = find_best();
        if (!best) return {-1, 0, false};

        return {best->pid, params_.quantum_us, false};
    }

    void on_cpu_used(Process* proc, int64_t cpu_time_us, int64_t /*current_time_us*/) override {
        // Advance pass by stride (once per scheduling quantum)
        // For partial quanta (preemption), scale proportionally
        double fraction = static_cast<double>(cpu_time_us) / params_.quantum_us;
        int64_t pass_advance = static_cast<int64_t>(proc->stride * fraction);
        proc->pass += std::max(pass_advance, int64_t(1));

        // Update global pass to track minimum
        update_global_pass();
    }

    bool should_preempt(Process* new_proc, Process* current_proc,
                        int64_t /*current_time_us*/) override {
        if (!current_proc) return true;
        // Stride doesn't preempt on arrival — processes wait for their turn
        // Preemption only happens via time slice expiration
        return false;
    }

    void reset() override {
        SingleQueueScheduler::reset();
        global_pass_ = 0;
    }

private:
    // Stride comparator: lowest pass value first, tiebreak by lower stride
    // (higher weight = lower stride = higher priority on tie)
    static bool stride_comparator(const Process* a, const Process* b) {
        if (a->pass != b->pass) return a->pass < b->pass;
        if (a->stride != b->stride) return a->stride < b->stride;
        return a->pid < b->pid;
    }

    void update_global_pass() {
        int64_t min_pass = std::numeric_limits<int64_t>::max();
        for (const auto* p : ready_queue()) {
            min_pass = std::min(min_pass, p->pass);
        }
        if (min_pass < std::numeric_limits<int64_t>::max()) {
            global_pass_ = std::max(global_pass_, min_pass);
        }
    }

    Params params_;
    int64_t global_pass_;
};
