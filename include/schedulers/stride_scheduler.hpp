/**
 * @file stride_scheduler.hpp
 * @brief Stride scheduling - deterministic proportional share
 *
 * Each task has a stride (inversely proportional to weight).
 * Select task with minimum pass value, then increment by stride.
 *
 * Reference: Waldspurger & Weihl (1995), "Stride Scheduling"
 */

#pragma once

#include "scheduler/single_queue_scheduler.hpp"
#include <algorithm>
#include <limits>

namespace sched_sim {

class StrideScheduler : public SingleQueueScheduler {
public:
    struct Params {
        double quantum;            // Fixed time quantum (ms)
        uint32_t stride_constant;
        Params() : quantum(5.0), stride_constant(10000) {}
    };

    explicit StrideScheduler(Params params = Params())
        : SingleQueueScheduler(stride_comparator),
          params_(params),
          global_pass_(0) {}

    std::string name() const override { return "Stride"; }

    void add_task(TaskPtr task, double /*current_time*/,
                  int /*preferred_core*/ = -1) override {
        uint32_t weight = task->weight();
        if (weight == 0) weight = 1;
        task->set_stride(std::max<uint32_t>(1, params_.stride_constant / weight));

        if (task->start_time() < 0) {
            // New task: set pass to global_pass
            task->set_pass_value(static_cast<uint32_t>(global_pass_));
        } else {
            // Returning: clamp pass to not fall too far behind
            task->set_pass_value(std::max(task->pass_value(),
                                          static_cast<uint32_t>(global_pass_)));
        }

        SingleQueueScheduler::add_task(task, 0);
    }

    ScheduleDecision schedule(double /*current_time*/, int /*core_id*/ = -1) override {
        TaskPtr best = find_best();
        if (!best) return {nullptr, 0.0};
        return {best, params_.quantum};
    }

    void on_cpu_used(TaskPtr task, double cpu_time, double /*current_time*/,
                     int /*core_id*/ = -1) override {
        double fraction = cpu_time / params_.quantum;
        uint32_t pass_advance = static_cast<uint32_t>(task->stride() * fraction);
        task->set_pass_value(task->pass_value() + std::max(pass_advance, uint32_t(1)));
        update_global_pass();
    }

    bool should_preempt(TaskPtr /*new_task*/, TaskPtr current_task,
                        double /*current_time*/, int /*core_id*/ = -1) override {
        if (!current_task) return true;
        // Stride doesn't preempt on arrival
        return false;
    }

    void reset() override {
        SingleQueueScheduler::reset();
        global_pass_ = 0;
    }

private:
    static bool stride_comparator(const TaskPtr& a, const TaskPtr& b) {
        if (a->pass_value() != b->pass_value()) return a->pass_value() < b->pass_value();
        if (a->stride() != b->stride()) return a->stride() < b->stride();
        return a->id() < b->id();
    }

    void update_global_pass() {
        uint64_t min_pass = std::numeric_limits<uint64_t>::max();
        for (const auto& t : ready_queue()) {
            min_pass = std::min(min_pass, static_cast<uint64_t>(t->pass_value()));
        }
        if (min_pass < std::numeric_limits<uint64_t>::max()) {
            global_pass_ = std::max(global_pass_, min_pass);
        }
    }

    Params params_;
    uint64_t global_pass_;
};

} // namespace sched_sim
