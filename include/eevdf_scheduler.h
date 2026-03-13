#pragma once

#include "single_queue_scheduler.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Earliest Eligible Virtual Deadline First (EEVDF) Scheduler
//
// Key idea: combines fairness (via virtual runtime) with latency guarantees
// (via virtual deadlines). Only "eligible" processes are considered, and
// among those, the one with the earliest virtual deadline runs.
//
// Virtual time model:
//   - Each process has a virtual eligible time (VET) and virtual deadline (VD)
//   - VET = vruntime (when the process became eligible for its next slice)
//   - VD  = VET + (request_length / weight) * total_weight
//   - A process is "eligible" when virtual_time >= VET
//
// Time slice = base_slice * (NICE_0_WEIGHT / weight)
//   where base_slice = sysctl_sched_base_slice (default 0.75ms in Linux 6.6+)
//
// The eligible process with the smallest virtual deadline is selected.
// This ensures that latency-sensitive (short-burst) processes get scheduled
// promptly even when competing with CPU-bound processes of equal weight.
//
// Reference: Stoica & Abdel-Wahab (1995), Zijlstra (2023), Linux 6.6+
class EEVDFScheduler : public SingleQueueScheduler {
public:
    struct Params {
        int64_t base_slice_us;
        Params() : base_slice_us(750) {}
    };

    explicit EEVDFScheduler(Params params = Params())
        : SingleQueueScheduler(eevdf_comparator),
          params_(params),
          virtual_time_(0.0) {}

    std::string name() const override {
        return "EEVDF (base_slice=" + std::to_string(params_.base_slice_us) + "us)";
    }

    void add_process(Process* proc, int64_t /*current_time_us*/) override {
        if (proc->first_run_time_us < 0) {
            // New process: set eligible time to current virtual time
            proc->eligible_time = virtual_time_;
            proc->vruntime = virtual_time_;
        } else {
            // Returning process: preserve eligible_time but clamp to avoid
            // accumulating too much credit from sleeping
            proc->eligible_time = std::max(proc->eligible_time, virtual_time_);
            proc->vruntime = std::max(proc->vruntime, virtual_time_);
        }

        // Compute virtual deadline for the upcoming request (slice)
        double request = calc_request(proc);
        proc->virtual_deadline = proc->eligible_time + request;

        SingleQueueScheduler::add_process(proc, 0);
    }

    ScheduleDecision schedule(int64_t /*current_time_us*/) override {
        // Find eligible process with earliest virtual deadline
        Process* best = nullptr;
        double best_deadline = std::numeric_limits<double>::max();

        for (auto* p : ready_queue()) {
            // Eligibility check: process is eligible if vruntime <= virtual_time
            if (p->eligible_time <= virtual_time_ + 1e-9) {
                if (p->virtual_deadline < best_deadline) {
                    best_deadline = p->virtual_deadline;
                    best = p;
                }
            }
        }

        // If no eligible process, pick the one with earliest eligible_time
        // (it will become eligible soonest)
        if (!best) {
            best = find_earliest_eligible();
        }

        if (!best) return {-1, 0, false};

        // Time slice: proportional to weight
        int64_t slice = calc_time_slice_us(best);
        return {best->pid, slice, false};
    }

    void on_cpu_used(Process* proc, int64_t cpu_time_us, int64_t /*current_time_us*/) override {
        // Advance virtual runtime for this process
        double delta_vruntime = static_cast<double>(cpu_time_us) *
                                NICE_0_WEIGHT / proc->weight;
        proc->vruntime += delta_vruntime;

        // Advance global virtual time
        // In real EEVDF, virtual_time advances at rate 1/total_weight per real time
        // For simplicity, track it as the minimum vruntime of all ready processes
        advance_virtual_time(cpu_time_us);

        // Update eligible time and deadline for next scheduling round
        proc->eligible_time = proc->vruntime;
        double request = calc_request(proc);
        proc->virtual_deadline = proc->eligible_time + request;
    }

    bool should_preempt(Process* new_proc, Process* current_proc,
                        int64_t /*current_time_us*/) override {
        if (!current_proc) return true;

        // Preempt if the new process has an earlier virtual deadline
        // and is eligible
        if (new_proc->eligible_time <= virtual_time_ + 1e-9) {
            return new_proc->virtual_deadline < current_proc->virtual_deadline;
        }
        return false;
    }

    void reset() override {
        SingleQueueScheduler::reset();
        virtual_time_ = 0.0;
    }

private:
    static constexpr double NICE_0_WEIGHT = 1024.0;

    // EEVDF comparator: among eligible, earliest virtual deadline first
    // Tie-break: smallest vruntime, then lowest pid
    static bool eevdf_comparator(const Process* a, const Process* b) {
        if (std::abs(a->virtual_deadline - b->virtual_deadline) > 1e-9)
            return a->virtual_deadline < b->virtual_deadline;
        if (std::abs(a->vruntime - b->vruntime) > 1e-9)
            return a->vruntime < b->vruntime;
        return a->pid < b->pid;
    }

    // Request size in virtual time units: how much virtual time one slice costs
    // request = slice_physical_time * total_weight_of_ready / weight
    // Simplified: request = base_slice * NICE_0_WEIGHT / weight
    double calc_request(const Process* proc) const {
        return static_cast<double>(params_.base_slice_us) * NICE_0_WEIGHT / proc->weight;
    }

    // Physical time slice: base_slice scaled by weight
    // Higher weight -> longer physical slice (but same virtual cost)
    int64_t calc_time_slice_us(const Process* proc) const {
        // slice = base_slice * weight / NICE_0_WEIGHT
        int64_t slice = static_cast<int64_t>(
            static_cast<double>(params_.base_slice_us) * proc->weight / NICE_0_WEIGHT
        );
        return std::max(slice, int64_t(100)); // minimum 100us
    }

    // Find process with earliest eligible_time (for when none are eligible yet)
    Process* find_earliest_eligible() const {
        Process* best = nullptr;
        double best_time = std::numeric_limits<double>::max();
        for (auto* p : ready_queue()) {
            if (p->eligible_time < best_time) {
                best_time = p->eligible_time;
                best = p;
            }
        }
        return best;
    }

    void advance_virtual_time(int64_t cpu_time_us) {
        // Virtual time advances by cpu_time / nr_running (weighted)
        int nr = ready_count();
        if (nr == 0) nr = 1;
        double total_weight = 0;
        for (const auto* p : ready_queue()) {
            total_weight += p->weight;
        }
        if (total_weight < 1.0) total_weight = NICE_0_WEIGHT;

        double advance = static_cast<double>(cpu_time_us) * NICE_0_WEIGHT / total_weight;
        virtual_time_ += advance;
    }

    Params params_;
    double virtual_time_;
};
