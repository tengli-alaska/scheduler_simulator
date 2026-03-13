#pragma once

#include "process.h"
#include "event.h"
#include "scheduler.h"
#include "metrics.h"
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cassert>

class Simulator {
public:
    Simulator(Scheduler* scheduler, std::vector<Process> processes)
        : scheduler_(scheduler), processes_(std::move(processes)) {}

    SimulationMetrics run(bool verbose = false) {
        reset();

        // Schedule all process arrivals
        for (auto& proc : processes_) {
            events_.push({proc.arrival_time_us, EventType::PROCESS_ARRIVAL, proc.pid});
            proc_map_[proc.pid] = &proc;
        }

        if (verbose) {
            std::cout << "Starting simulation with " << processes_.size()
                      << " processes using " << scheduler_->name() << std::endl;
        }

        // Main event loop
        while (!events_.empty()) {
            Event event = events_.pop();
            int64_t prev_time = current_time_us_;
            current_time_us_ = event.time_us;

            // Track idle time
            if (running_pid_ == -1 && current_time_us_ > prev_time) {
                metrics_.total_idle_time_us += (current_time_us_ - prev_time);
            }

            switch (event.type) {
                case EventType::PROCESS_ARRIVAL:
                    handle_arrival(event);
                    break;
                case EventType::CPU_BURST_COMPLETE:
                    handle_burst_complete(event);
                    break;
                case EventType::TIME_SLICE_EXPIRE:
                    handle_time_slice_expire(event);
                    break;
                case EventType::IO_COMPLETE:
                    handle_io_complete(event);
                    break;
                case EventType::MLFQ_PRIORITY_BOOST:
                    handle_priority_boost(event);
                    break;
            }
        }

        // Collect final metrics
        finalize_metrics();

        if (verbose) {
            print_results();
        }

        return metrics_;
    }

private:
    void reset() {
        current_time_us_ = 0;
        running_pid_ = -1;
        running_start_us_ = 0;
        metrics_ = SimulationMetrics{};
        proc_map_.clear();
        scheduler_->reset();

        // Reset process state
        for (auto& proc : processes_) {
            proc.current_burst_index = 0;
            proc.remaining_burst_us = proc.cpu_bursts_us.empty() ? 0 : proc.cpu_bursts_us[0];
            proc.total_cpu_time_us = 0;
            proc.total_wait_time_us = 0;
            proc.enter_ready_time_us = 0;
            proc.first_run_time_us = -1;
            proc.completion_time_us = -1;
            proc.vruntime = 0;
            proc.virtual_deadline = 0;
            proc.eligible_time = 0;
            proc.pass = 0;
            proc.mlfq_queue_level = 0;
            proc.mlfq_allotment_remaining_us = 0;
        }
    }

    void handle_arrival(const Event& event) {
        Process* proc = proc_map_[event.pid];
        proc->enter_ready_time_us = current_time_us_;

        // Check if this new process should preempt the running one
        Process* running = (running_pid_ >= 0) ? proc_map_[running_pid_] : nullptr;

        scheduler_->add_process(proc, current_time_us_);

        if (running_pid_ == -1) {
            // CPU idle, dispatch immediately
            dispatch_next();
        } else if (scheduler_->should_preempt(proc, running, current_time_us_)) {
            preempt_current();
            dispatch_next();
        }
    }

    void handle_burst_complete(const Event& event) {
        if (running_pid_ != event.pid) return; // stale event

        Process* proc = proc_map_[event.pid];

        // Account for CPU time used
        int64_t cpu_used = current_time_us_ - running_start_us_;
        proc->total_cpu_time_us += cpu_used;
        proc->remaining_burst_us = 0;
        scheduler_->on_cpu_used(proc, cpu_used, current_time_us_);

        running_pid_ = -1;

        // Check if there's an IO burst to do
        if (proc->current_burst_index < static_cast<int>(proc->io_bursts_us.size()) &&
            proc->io_bursts_us[proc->current_burst_index] > 0) {
            // Start IO
            int64_t io_time = proc->io_bursts_us[proc->current_burst_index];
            events_.push({current_time_us_ + io_time, EventType::IO_COMPLETE, proc->pid});
        } else {
            // Move to next CPU burst or complete
            proc->current_burst_index++;
            if (proc->is_completed()) {
                proc->completion_time_us = current_time_us_;
            } else {
                // More CPU bursts to do (multi-burst process)
                proc->remaining_burst_us = proc->current_cpu_burst_us();
                proc->enter_ready_time_us = current_time_us_;
                scheduler_->add_process(proc, current_time_us_);
            }
        }

        dispatch_next();
    }

    void handle_time_slice_expire(const Event& event) {
        if (running_pid_ != event.pid) return; // stale event

        Process* proc = proc_map_[event.pid];

        // Account for CPU time used (the full time slice)
        int64_t cpu_used = current_time_us_ - running_start_us_;
        proc->total_cpu_time_us += cpu_used;
        proc->remaining_burst_us -= cpu_used;
        scheduler_->on_cpu_used(proc, cpu_used, current_time_us_);

        running_pid_ = -1;
        metrics_.total_context_switches++;

        // Put back in ready queue
        proc->enter_ready_time_us = current_time_us_;
        scheduler_->add_process(proc, current_time_us_);

        dispatch_next();
    }

    void handle_io_complete(const Event& event) {
        Process* proc = proc_map_[event.pid];

        // Advance to next burst
        proc->current_burst_index++;
        if (proc->is_completed()) {
            proc->completion_time_us = current_time_us_;
            // Don't dispatch — just let the current process continue
            if (running_pid_ == -1) dispatch_next();
            return;
        }

        proc->remaining_burst_us = proc->current_cpu_burst_us();
        proc->enter_ready_time_us = current_time_us_;

        Process* running = (running_pid_ >= 0) ? proc_map_[running_pid_] : nullptr;
        scheduler_->add_process(proc, current_time_us_);

        if (running_pid_ == -1) {
            dispatch_next();
        } else if (scheduler_->should_preempt(proc, running, current_time_us_)) {
            preempt_current();
            dispatch_next();
        }
    }

    void handle_priority_boost(const Event& event) {
        // MLFQ-specific: handled by the scheduler's on_cpu_used or a callback
        // For now, this is a placeholder that the MLFQ scheduler will use
        // to reset all processes to the top queue
    }

    void preempt_current() {
        if (running_pid_ < 0) return;

        Process* proc = proc_map_[running_pid_];
        int64_t cpu_used = current_time_us_ - running_start_us_;
        proc->total_cpu_time_us += cpu_used;
        proc->remaining_burst_us -= cpu_used;
        scheduler_->on_cpu_used(proc, cpu_used, current_time_us_);

        // Cancel pending completion/slice events for this process
        events_.cancel(running_pid_, EventType::CPU_BURST_COMPLETE);
        events_.cancel(running_pid_, EventType::TIME_SLICE_EXPIRE);

        // Put back in ready queue
        proc->enter_ready_time_us = current_time_us_;
        scheduler_->add_process(proc, current_time_us_);

        metrics_.total_context_switches++;
        running_pid_ = -1;
    }

    void dispatch_next() {
        if (scheduler_->ready_count() == 0) {
            running_pid_ = -1;
            return;
        }

        ScheduleDecision decision = scheduler_->schedule(current_time_us_);
        if (decision.pid < 0) {
            running_pid_ = -1;
            return;
        }

        Process* proc = proc_map_[decision.pid];
        scheduler_->remove_process(proc->pid);

        // Track wait time
        proc->total_wait_time_us += (current_time_us_ - proc->enter_ready_time_us);

        // Track first run
        if (proc->first_run_time_us < 0) {
            proc->first_run_time_us = current_time_us_;
        }

        running_pid_ = proc->pid;
        running_start_us_ = current_time_us_;

        if (running_pid_ != decision.pid) {
            metrics_.total_context_switches++;
        }

        // Schedule completion or preemption event
        int64_t burst_remaining = proc->remaining_burst_us;
        int64_t time_slice = decision.time_slice_us;

        if (time_slice > 0 && time_slice < burst_remaining) {
            // Will be preempted before burst completes
            events_.push({current_time_us_ + time_slice,
                         EventType::TIME_SLICE_EXPIRE, proc->pid});
        } else {
            // Burst will complete within this scheduling round
            events_.push({current_time_us_ + burst_remaining,
                         EventType::CPU_BURST_COMPLETE, proc->pid});
        }
    }

    void finalize_metrics() {
        metrics_.total_simulation_time_us = current_time_us_;
        metrics_.total_processes = static_cast<int>(processes_.size());

        for (const auto& proc : processes_) {
            if (proc.completion_time_us >= 0) {
                metrics_.completed_processes++;
                metrics_.response_times_us.push_back(proc.response_time_us());
                metrics_.turnaround_times_us.push_back(proc.turnaround_time_us());
                metrics_.wait_times_us.push_back(proc.total_wait_time_us);
                metrics_.cpu_times_us.push_back(proc.total_cpu_time_us);
            }
        }
    }

    void print_results() const {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Simulation Results: " << scheduler_->name() << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total processes:      " << metrics_.total_processes << std::endl;
        std::cout << "Completed:            " << metrics_.completed_processes << std::endl;
        std::cout << "Simulation time:      " << metrics_.total_simulation_time_us / 1e6
                  << " s" << std::endl;
        std::cout << "Throughput:           " << metrics_.throughput()
                  << " tasks/s" << std::endl;
        std::cout << "CPU utilization:      " << metrics_.cpu_utilization() * 100
                  << " %" << std::endl;
        std::cout << "Avg response time:    " << metrics_.avg_response_time_us() / 1e3
                  << " ms" << std::endl;
        std::cout << "Avg turnaround time:  " << metrics_.avg_turnaround_time_us() / 1e3
                  << " ms" << std::endl;
        std::cout << "Avg wait time:        " << metrics_.avg_wait_time_us() / 1e3
                  << " ms" << std::endl;
        std::cout << "Max wait time:        " << metrics_.max_wait_time_us() / 1e3
                  << " ms" << std::endl;
        std::cout << "P95 response time:    "
                  << metrics_.percentile_response_time_us(0.95) / 1e3 << " ms" << std::endl;
        std::cout << "P99 response time:    "
                  << metrics_.percentile_response_time_us(0.99) / 1e3 << " ms" << std::endl;
        std::cout << "Context switches:     " << metrics_.total_context_switches << std::endl;
        std::cout << "Context switch rate:  " << metrics_.context_switch_rate()
                  << " /s" << std::endl;
        std::cout << "Jain's fairness:      " << metrics_.jains_fairness_index(processes_)
                  << std::endl;
        std::cout << "========================================" << std::endl;
    }

    Scheduler* scheduler_;
    std::vector<Process> processes_;
    std::unordered_map<int, Process*> proc_map_;
    EventQueue events_;

    int64_t current_time_us_ = 0;
    int running_pid_ = -1;
    int64_t running_start_us_ = 0;
    SimulationMetrics metrics_;
};
