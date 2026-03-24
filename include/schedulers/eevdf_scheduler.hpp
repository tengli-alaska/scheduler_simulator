/**
 * @file eevdf_scheduler.hpp
 * @brief Earliest Eligible Virtual Deadline First scheduler
 *
 * Combines fairness (via virtual runtime) with latency guarantees
 * (via virtual deadlines). Only "eligible" tasks are considered, and
 * among those, the one with the earliest virtual deadline runs.
 *
 * Reference: Stoica & Abdel-Wahab (1995), Zijlstra (2023), Linux 6.6+
 */

#pragma once

#include "scheduler/single_queue_scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sched_sim {

class EEVDFScheduler : public SingleQueueScheduler {
public:
    struct Params {
        double base_slice;   // Base time slice (ms)
        Params() : base_slice(3.0) {}
    };

    explicit EEVDFScheduler(Params params = Params())
        : SingleQueueScheduler(eevdf_comparator),
          params_(params),
          virtual_time_(0.0) {}

    std::string name() const override { return "EEVDF"; }

    void add_task(TaskPtr task, double /*current_time*/,
                  int /*preferred_core*/ = -1) override {
        if (task->start_time() < 0) {
            task->set_eligible_time(virtual_time_);
            task->set_vruntime(virtual_time_);
        } else {
            task->set_eligible_time(std::max(task->eligible_time(), virtual_time_));
            task->set_vruntime(std::max(task->vruntime(), virtual_time_));
        }

        // Compute virtual deadline for the upcoming request (slice)
        double request = calc_request(task);
        task->set_deadline(task->eligible_time() + request);

        SingleQueueScheduler::add_task(task, 0);
    }

    ScheduleDecision schedule(double /*current_time*/, int /*core_id*/ = -1) override {
        // Find eligible task with earliest virtual deadline
        TaskPtr best = nullptr;
        double best_deadline = std::numeric_limits<double>::max();

        for (auto& t : ready_queue()) {
            if (t->eligible_time() <= virtual_time_ + 1e-9) {
                if (t->deadline() < best_deadline) {
                    best_deadline = t->deadline();
                    best = t;
                }
            }
        }

        // If no eligible task, pick the one with earliest eligible_time
        if (!best) {
            best = find_earliest_eligible();
        }

        if (!best) return {nullptr, 0.0};

        double slice = calc_time_slice(best);
        return {best, slice};
    }

    void on_cpu_used(TaskPtr task, double cpu_time, double /*current_time*/,
                     int /*core_id*/ = -1) override {
        double delta_vruntime = cpu_time * NICE_0_LOAD / static_cast<double>(task->weight());
        task->set_vruntime(task->vruntime() + delta_vruntime);

        advance_virtual_time(cpu_time);

        task->set_eligible_time(task->vruntime());
        double request = calc_request(task);
        task->set_deadline(task->eligible_time() + request);
    }

    bool should_preempt(TaskPtr new_task, TaskPtr current_task,
                        double /*current_time*/, int /*core_id*/ = -1) override {
        if (!current_task) return true;
        if (new_task->eligible_time() <= virtual_time_ + 1e-9) {
            return new_task->deadline() < current_task->deadline();
        }
        return false;
    }

    void reset() override {
        SingleQueueScheduler::reset();
        virtual_time_ = 0.0;
    }

private:
    static bool eevdf_comparator(const TaskPtr& a, const TaskPtr& b) {
        if (std::abs(a->deadline() - b->deadline()) > 1e-9)
            return a->deadline() < b->deadline();
        if (std::abs(a->vruntime() - b->vruntime()) > 1e-9)
            return a->vruntime() < b->vruntime();
        return a->id() < b->id();
    }

    double calc_request(const TaskPtr& task) const {
        return params_.base_slice * NICE_0_LOAD / static_cast<double>(task->weight());
    }

    double calc_time_slice(const TaskPtr& task) const {
        double slice = params_.base_slice * task->weight() / static_cast<double>(NICE_0_LOAD);
        return std::max(slice, 0.1); // minimum 0.1ms
    }

    TaskPtr find_earliest_eligible() const {
        TaskPtr best = nullptr;
        double best_time = std::numeric_limits<double>::max();
        for (auto& t : ready_queue()) {
            if (t->eligible_time() < best_time) {
                best_time = t->eligible_time();
                best = t;
            }
        }
        return best;
    }

    void advance_virtual_time(double cpu_time) {
        int nr = ready_count();
        if (nr == 0) nr = 1;
        double total_weight = 0;
        for (const auto& t : ready_queue()) {
            total_weight += t->weight();
        }
        if (total_weight < 1.0) total_weight = NICE_0_LOAD;
        double advance = cpu_time * NICE_0_LOAD / total_weight;
        virtual_time_ += advance;
    }

    Params params_;
    double virtual_time_;
};

} // namespace sched_sim
