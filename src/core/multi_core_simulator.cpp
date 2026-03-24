#include "scheduler/multi_core_simulator.hpp"
#include <iostream>
#include <algorithm>

namespace sched_sim {

MultiCoreSimulator::MultiCoreSimulator(
    SchedulerFactory scheduler_factory,
    int num_cores,
    LoadBalancerPtr balancer,
    bool enable_work_stealing,
    double default_time_slice)
    : balancer_(std::move(balancer))
    , num_cores_(num_cores)
    , enable_work_stealing_(enable_work_stealing)
    , default_time_slice_(default_time_slice)
{
    // Create one scheduler instance per core
    for (int i = 0; i < num_cores_; ++i) {
        schedulers_.push_back(scheduler_factory());
    }
    running_tasks_.resize(num_cores_);
    last_event_time_.resize(num_cores_);
}

void MultiCoreSimulator::add_task(TaskPtr task) {
    all_tasks_.push_back(task);
    events_.schedule(EventType::ARRIVAL, task->arrival_time(), task, 0);
}

void MultiCoreSimulator::run(double stop_time) {
    stop_time_ = stop_time;

    std::cout << "Starting multi-queue simulation (cores=" << num_cores_
              << ", balancer=" << balancer_->name()
              << ", work_stealing=" << (enable_work_stealing_ ? "on" : "off")
              << ", stop_time=" << stop_time << ")...\n";

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
              << "/" << all_tasks_.size() << " tasks completed"
              << " (work steals: " << work_steals_ << ")\n";
}

void MultiCoreSimulator::handle_arrival(const Event& evt) {
    auto task = evt.task;

    // Load balancer decides which core gets this task
    int target_core = balancer_->assign(task, schedulers_, running_tasks_);

    // Add to that core's scheduler
    schedulers_[target_core]->add_task(task, current_time_, target_core);

    // If target core is idle, dispatch immediately
    if (!running_tasks_[target_core]) {
        dispatch_next(target_core);
        return;
    }

    // Check if new task should preempt the running task on target core
    if (schedulers_[target_core]->should_preempt(task, running_tasks_[target_core],
                                                 current_time_, target_core)) {
        preempt_core(target_core);
        dispatch_next(target_core);
    }
}

void MultiCoreSimulator::handle_time_slice(const Event& evt) {
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

    // Notify this core's scheduler of CPU usage
    schedulers_[core_id]->on_cpu_used(task, elapsed, current_time_, core_id);

    // Check if completed
    if (task->is_completed()) {
        task->complete(current_time_);
        completed_tasks_.push_back(task);
        running_tasks_[core_id] = nullptr;
    } else {
        // Time slice expired — put back in this core's ready queue
        task->increment_preemptions();
        preemptions_++;
        running_tasks_[core_id] = nullptr;
        schedulers_[core_id]->add_task(task, current_time_, core_id);
    }

    // Try to dispatch next task from this core's queue
    dispatch_next(core_id);
}

void MultiCoreSimulator::preempt_core(int core_id) {
    auto task = running_tasks_[core_id];
    if (!task) return;

    // Account for CPU time used so far
    double elapsed = current_time_ - last_event_time_[core_id];
    task->execute(elapsed);
    schedulers_[core_id]->on_cpu_used(task, elapsed, current_time_, core_id);

    // Cancel pending time slice event
    events_.cancel_task(task);

    // Put back in this core's ready queue
    task->increment_preemptions();
    preemptions_++;
    running_tasks_[core_id] = nullptr;
    schedulers_[core_id]->add_task(task, current_time_, core_id);

    context_switches_++;
}

void MultiCoreSimulator::dispatch_next(int core_id) {
    // First try this core's own queue
    if (schedulers_[core_id]->ready_count() > 0) {
        ScheduleDecision decision = schedulers_[core_id]->schedule(current_time_, core_id);
        if (decision.task) {
            TaskPtr task = decision.task;
            schedulers_[core_id]->remove_task(task->id());

            running_tasks_[core_id] = task;
            task->start(current_time_);
            last_event_time_[core_id] = current_time_;
            context_switches_++;

            double slice = (decision.time_slice > 0) ? decision.time_slice : default_time_slice_;
            double remaining = task->remaining_time();
            double event_time = current_time_ + std::min(slice, remaining);

            events_.schedule(EventType::TIME_SLICE, event_time, task, core_id);
            return;
        }
    }

    // Queue empty — try work stealing if enabled
    if (enable_work_stealing_ && try_work_steal(core_id)) {
        return;
    }

    running_tasks_[core_id] = nullptr;
}

bool MultiCoreSimulator::try_work_steal(int idle_core) {
    // Find the busiest core (most tasks in ready queue)
    int busiest_core = -1;
    int max_ready = 1; // Need at least 2 tasks (1 to keep, 1 to steal)

    for (int i = 0; i < num_cores_; ++i) {
        if (i == idle_core) continue;
        int ready = schedulers_[i]->ready_count();
        if (ready > max_ready) {
            max_ready = ready;
            busiest_core = i;
        }
    }

    if (busiest_core < 0) return false;

    // Steal the next-to-run task from the busiest core
    ScheduleDecision decision = schedulers_[busiest_core]->schedule(current_time_, busiest_core);
    if (!decision.task) return false;

    TaskPtr stolen = decision.task;
    schedulers_[busiest_core]->remove_task(stolen->id());

    // Add to idle core's scheduler and dispatch
    schedulers_[idle_core]->add_task(stolen, current_time_, idle_core);

    work_steals_++;

    // Now dispatch from idle core's queue
    ScheduleDecision local_decision = schedulers_[idle_core]->schedule(current_time_, idle_core);
    if (!local_decision.task) return false;

    TaskPtr task = local_decision.task;
    schedulers_[idle_core]->remove_task(task->id());

    running_tasks_[idle_core] = task;
    task->start(current_time_);
    last_event_time_[idle_core] = current_time_;
    context_switches_++;

    double slice = (local_decision.time_slice > 0) ? local_decision.time_slice : default_time_slice_;
    double remaining = task->remaining_time();
    double event_time = current_time_ + std::min(slice, remaining);

    events_.schedule(EventType::TIME_SLICE, event_time, task, idle_core);

    return true;
}

} // namespace sched_sim
