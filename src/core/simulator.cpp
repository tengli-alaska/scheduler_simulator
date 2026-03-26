#include "scheduler/simulator.hpp"
#include <iostream>
#include <algorithm>

namespace sched_sim {

Simulator::Simulator(SchedulerPtr scheduler, int num_cores, double default_time_slice)
    : scheduler_(std::move(scheduler))
    , num_cores_(num_cores)
    , default_time_slice_(default_time_slice)
{
    running_tasks_.resize(num_cores_);
    last_event_time_.resize(num_cores_);
}

void Simulator::add_task(TaskPtr task) {
    all_tasks_.push_back(task);
    events_.schedule(EventType::ARRIVAL, task->arrival_time(), task, 0);
}

void Simulator::run(double stop_time) {
    stop_time_ = stop_time;

    std::cout << "Starting simulation (stop_time=" << stop_time << ")...\n";

    while (auto evt_opt = events_.get_next()) {
        auto evt = *evt_opt;

        if (evt.time > stop_time && completed_tasks_.size() >= all_tasks_.size()) {
            break;
        }

        current_time_ = evt.time;

        switch (evt.type) {
            case EventType::ARRIVAL:
                handle_arrival(evt);
                break;
            case EventType::TIME_SLICE:
                handle_time_slice(evt);
                break;
            default:
                break;
        }
    }

    std::cout << "Simulation complete: " << completed_tasks_.size()
              << "/" << all_tasks_.size() << " tasks completed\n";
}

void Simulator::handle_arrival(const Event& evt) {
    auto task = evt.task;

    // Add to scheduler's ready queue
    scheduler_->add_task(task, current_time_, -1);

    // Check each core: find idle core or check preemption
    for (int core_id = 0; core_id < num_cores_; ++core_id) {
        if (!running_tasks_[core_id]) {
            dispatch_next(core_id);
            return;
        }
    }

    // All cores busy — check if new task should preempt any running task
    for (int core_id = 0; core_id < num_cores_; ++core_id) {
        if (scheduler_->should_preempt(task, running_tasks_[core_id], current_time_, core_id)) {
            preempt_core(core_id);
            dispatch_next(core_id);
            return;
        }
    }
}

void Simulator::handle_time_slice(const Event& evt) {
    int core_id = evt.core_id;
    auto task = running_tasks_[core_id];

    // Verify task is still running on this core
    if (!task || task != evt.task) {
        return;
    }

    // Calculate elapsed time
    double elapsed = current_time_ - last_event_time_[core_id];

    // Execute task
    task->execute(elapsed);

    // Notify scheduler of CPU usage
    scheduler_->on_cpu_used(task, elapsed, current_time_, core_id);

    // Check if completed
    if (task->is_completed()) {
        task->complete(current_time_);
        completed_tasks_.push_back(task);
        running_tasks_[core_id] = nullptr;
    } else {
        // Time slice expired — put back in ready queue
        task->increment_preemptions();
        preemptions_++;
        running_tasks_[core_id] = nullptr;
        scheduler_->add_task(task, current_time_, core_id);
    }

    // Schedule next task on this core
    dispatch_next(core_id);
}

void Simulator::preempt_core(int core_id) {
    auto task = running_tasks_[core_id];
    if (!task) return;

    // Account for CPU time used so far
    double elapsed = current_time_ - last_event_time_[core_id];
    task->execute(elapsed);
    scheduler_->on_cpu_used(task, elapsed, current_time_, core_id);

    // Cancel pending time slice event for this task
    events_.cancel_task(task);

    // Put back in ready queue
    task->increment_preemptions();
    preemptions_++;
    running_tasks_[core_id] = nullptr;
    scheduler_->add_task(task, current_time_, core_id);

    context_switches_++;
}

void Simulator::dispatch_next(int core_id) {
    if (scheduler_->ready_count() == 0) {
        running_tasks_[core_id] = nullptr;
        return;
    }

    // Ask scheduler for next task and time slice
    ScheduleDecision decision = scheduler_->schedule(current_time_, core_id);
    if (!decision.task) {
        running_tasks_[core_id] = nullptr;
        return;
    }

    TaskPtr task = decision.task;

    // Remove from ready queue
    scheduler_->remove_task(task->id());

    // Dispatch
    running_tasks_[core_id] = task;
    task->start(current_time_);
    last_event_time_[core_id] = current_time_;

    context_switches_++;

    // Use scheduler's time slice, or default if not provided
    double slice = (decision.time_slice > 0) ? decision.time_slice : default_time_slice_;

    // Don't schedule beyond what the task needs
    double remaining = task->remaining_time();
    double event_time = current_time_ + std::min(slice, remaining);

    events_.schedule(EventType::TIME_SLICE, event_time, task, core_id);
}

} // namespace sched_sim
