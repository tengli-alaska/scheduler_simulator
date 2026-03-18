/**
 * @file cfs_scheduler.hpp
 * @brief Completely Fair Scheduler (CFS) implementation
 *
 * CFS uses virtual runtime (vruntime) to provide fair CPU time distribution.
 * Tasks with lower vruntime get selected first.
 *
 * vruntime += delta_exec * (NICE_0_WEIGHT / weight)
 * Time slice = max(sched_latency / nr_running, min_granularity)
 *
 * Reference: Molnar (2007), Linux kernel sched_fair.c
 */

#pragma once

#include "scheduler/single_queue_scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sched_sim {

class CFSScheduler : public SingleQueueScheduler {
public:
    struct Params {
        double sched_latency;        // Target scheduling latency (ms)
        double min_granularity;      // Minimum time slice (ms)
        double wakeup_granularity;   // Preemption threshold (ms)
        Params() : sched_latency(24.0), min_granularity(3.0),
                   wakeup_granularity(4.0) {}
    };

    explicit CFSScheduler(Params params = Params())
        : SingleQueueScheduler(cfs_comparator),
          params_(params),
          min_vruntime_(0.0) {}

    std::string name() const override { return "CFS"; }

    void add_task(TaskPtr task, double current_time) override {
        if (task->start_time() < 0) {
            // Brand new task: set vruntime to current min to be fair
            task->set_vruntime(min_vruntime_);
        } else {
            // Returning from preemption: clamp vruntime to prevent credit hoarding
            double max_credit = params_.sched_latency *
                                NICE_0_LOAD / static_cast<double>(task->weight());
            task->set_vruntime(std::max(task->vruntime(), min_vruntime_ - max_credit));
        }
        SingleQueueScheduler::add_task(task, current_time);
    }

    ScheduleDecision schedule(double /*current_time*/) override {
        TaskPtr best = find_best();
        if (!best) return {nullptr, 0.0};

        int nr_running = ready_count();
        double time_slice = calc_time_slice(best, nr_running);

        return {best, time_slice};
    }

    void on_cpu_used(TaskPtr task, double cpu_time, double /*current_time*/) override {
        double delta_vruntime = cpu_time * NICE_0_LOAD / static_cast<double>(task->weight());
        task->set_vruntime(task->vruntime() + delta_vruntime);
        update_min_vruntime();
    }

    bool should_preempt(TaskPtr new_task, TaskPtr current_task,
                        double /*current_time*/) override {
        if (!current_task) return true;
        double gran = params_.wakeup_granularity *
                      NICE_0_LOAD / static_cast<double>(current_task->weight());
        return new_task->vruntime() + gran < current_task->vruntime();
    }

    void reset() override {
        SingleQueueScheduler::reset();
        min_vruntime_ = 0.0;
    }

private:
    static bool cfs_comparator(const TaskPtr& a, const TaskPtr& b) {
        if (std::abs(a->vruntime() - b->vruntime()) > 1e-9)
            return a->vruntime() < b->vruntime();
        return a->id() < b->id();
    }

    double calc_time_slice(const TaskPtr& task, int nr_running) const {
        if (nr_running <= 0) nr_running = 1;

        double period = params_.sched_latency;
        if (nr_running > static_cast<int>(period / params_.min_granularity)) {
            period = nr_running * params_.min_granularity;
        }

        double total_weight = task->weight();
        for (const auto& t : ready_queue()) {
            total_weight += t->weight();
        }

        double slice = period * task->weight() / total_weight;
        return std::max(slice, params_.min_granularity);
    }

    void update_min_vruntime() {
        double new_min = std::numeric_limits<double>::max();
        for (const auto& t : ready_queue()) {
            new_min = std::min(new_min, t->vruntime());
        }
        if (new_min < std::numeric_limits<double>::max()) {
            min_vruntime_ = std::max(min_vruntime_, new_min);
        }
    }

    Params params_;
    double min_vruntime_;
};

} // namespace sched_sim
