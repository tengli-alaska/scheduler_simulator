#include "scheduler/simulator.hpp"
#include <iostream>
#include <algorithm>

namespace sched_sim {

Simulator::Simulator(SchedulerPtr scheduler, int num_cores, double time_slice)
    : scheduler_(std::move(scheduler))
    , num_cores_(num_cores)
    , time_slice_(time_slice)
{
    running_tasks_.resize(num_cores_);
    last_event_time_.resize(num_cores_);
    
    scheduler_->initialize(this);
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
    
    // Submit to scheduler
    scheduler_->submit_task(task);
    
    // Find idle core
    for (int core_id = 0; core_id < num_cores_; ++core_id) {
        if (!running_tasks_[core_id]) {
            schedule_task_on_core(core_id);
            break;
        }
    }
}

void Simulator::handle_time_slice(const Event& evt) {
    int core_id = evt.core_id;
    auto task = running_tasks_[core_id];
    
    // Verify task is still running
    if (!task || task != evt.task) {
        return;
    }
    
    // Calculate elapsed time
    double elapsed = current_time_ - last_event_time_[core_id];
    
    // Execute task
    task->execute(elapsed);
    
    // Notify scheduler
    scheduler_->task_tick(task, elapsed, core_id);
    
    // Check if completed
    if (task->is_completed()) {
        task->complete(current_time_);
        completed_tasks_.push_back(task);
        running_tasks_[core_id] = nullptr;
        scheduler_->task_completed(task);
    } else {
        task->increment_preemptions();
        running_tasks_[core_id] = nullptr;
    }
    
    // Schedule next task on this core
    schedule_task_on_core(core_id);
}

void Simulator::schedule_task_on_core(int core_id) {
    auto task = scheduler_->select_next(core_id);
    
    if (task) {
        running_tasks_[core_id] = task;
        task->start(current_time_);
        last_event_time_[core_id] = current_time_;
        
        // Schedule time slice expiration
        double next_event = current_time_ + time_slice_;
        events_.schedule(EventType::TIME_SLICE, next_event, task, core_id);
    }
}

} // namespace sched_sim