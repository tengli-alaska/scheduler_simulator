#pragma once

#include "single_queue_scheduler.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Completely Fair Scheduler (CFS)
//
// Key idea: each process accumulates "virtual runtime" (vruntime) inversely
// proportional to its weight. The process with the smallest vruntime runs next.
//
// vruntime += delta_exec * (NICE_0_WEIGHT / weight)
//
// Time slice = max(sched_latency / nr_running, min_granularity)
// where sched_latency = target scheduling period for one full round.
//
// Preemption: a newly waking process preempts the current one if its vruntime
// is smaller by at least min_granularity worth of virtual time.
//
// Reference: Molnár (2007), Linux kernel sched_fair.c
class CFSScheduler : public SingleQueueScheduler {
public:
    struct Params {
        int64_t sched_latency_us;
        int64_t min_granularity_us;
        int64_t wakeup_granularity_us;
        Params() : sched_latency_us(6000), min_granularity_us(750),
                   wakeup_granularity_us(1000) {}
    };

    explicit CFSScheduler(Params params = Params())
        : SingleQueueScheduler(cfs_comparator),
          params_(params),
          min_vruntime_(0.0) {}

    std::string name() const override {
        return "CFS (latency=" + std::to_string(params_.sched_latency_us / 1000) + "ms)";
    }

    void add_process(Process* proc, int64_t current_time_us) override {
        // New processes start at min_vruntime to avoid unfair advantage
        // Returning processes keep their vruntime (already set)
        if (proc->first_run_time_us < 0) {
            // Brand new process: set vruntime to current min to be fair
            proc->vruntime = min_vruntime_;
        } else {
            // Returning from IO or preemption: ensure vruntime >= min_vruntime - latency
            // This prevents processes from accumulating credit while sleeping
            double max_credit = static_cast<double>(params_.sched_latency_us) *
                                NICE_0_WEIGHT / proc->weight;
            proc->vruntime = std::max(proc->vruntime, min_vruntime_ - max_credit);
        }
        SingleQueueScheduler::add_process(proc, current_time_us);
    }

    ScheduleDecision schedule(int64_t /*current_time_us*/) override {
        Process* best = find_best();
        if (!best) return {-1, 0, false};

        int nr_running = ready_count();
        int64_t time_slice = calc_time_slice(best, nr_running);

        return {best->pid, time_slice, false};
    }

    void on_cpu_used(Process* proc, int64_t cpu_time_us, int64_t /*current_time_us*/) override {
        // Update vruntime: weighted by inverse of process weight
        double delta_vruntime = static_cast<double>(cpu_time_us) *
                                NICE_0_WEIGHT / proc->weight;
        proc->vruntime += delta_vruntime;

        // Update min_vruntime (monotonically increasing)
        update_min_vruntime();
    }

    bool should_preempt(Process* new_proc, Process* current_proc,
                        int64_t /*current_time_us*/) override {
        if (!current_proc) return true;

        // Preempt if new process's vruntime is smaller by at least wakeup_granularity
        double gran = static_cast<double>(params_.wakeup_granularity_us) *
                      NICE_0_WEIGHT / current_proc->weight;
        return new_proc->vruntime + gran < current_proc->vruntime;
    }

    void reset() override {
        SingleQueueScheduler::reset();
        min_vruntime_ = 0.0;
    }

private:
    static constexpr double NICE_0_WEIGHT = 1024.0;

    // CFS comparator: smallest vruntime first, tiebreak by pid
    static bool cfs_comparator(const Process* a, const Process* b) {
        if (std::abs(a->vruntime - b->vruntime) > 1e-9)
            return a->vruntime < b->vruntime;
        return a->pid < b->pid;
    }

    // Time slice calculation: proportional to weight
    // slice_i = max(sched_latency * weight_i / total_weight, min_granularity)
    int64_t calc_time_slice(const Process* proc, int nr_running) const {
        if (nr_running <= 0) nr_running = 1;

        // If many processes, extend latency to ensure min_granularity each
        int64_t period = params_.sched_latency_us;
        if (nr_running > static_cast<int>(period / params_.min_granularity_us)) {
            period = static_cast<int64_t>(nr_running) * params_.min_granularity_us;
        }

        // Sum weights of all ready processes + this one
        double total_weight = proc->weight;
        for (const auto* p : ready_queue()) {
            total_weight += p->weight;
        }

        int64_t slice = static_cast<int64_t>(
            static_cast<double>(period) * proc->weight / total_weight
        );

        return std::max(slice, params_.min_granularity_us);
    }

    void update_min_vruntime() {
        double new_min = std::numeric_limits<double>::max();
        for (const auto* p : ready_queue()) {
            new_min = std::min(new_min, p->vruntime);
        }
        if (new_min < std::numeric_limits<double>::max()) {
            min_vruntime_ = std::max(min_vruntime_, new_min);
        }
    }

    Params params_;
    double min_vruntime_;
};
