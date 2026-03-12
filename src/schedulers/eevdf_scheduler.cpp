#include "schedulers/eevdf_scheduler.hpp"
#include <algorithm>
#include <limits>

namespace sched_sim {

EEVDFScheduler::EEVDFScheduler(double base_slice)
    : base_slice_(base_slice)
{
}

double EEVDFScheduler::calc_delta_fair(double delta, uint32_t weight) noexcept {
    return delta * NICE_0_LOAD / static_cast<double>(weight);
}

bool EEVDFScheduler::is_eligible(const TaskPtr& task) const noexcept {
    return task->vruntime() <= min_vruntime_;
}

void EEVDFScheduler::update_deadline(TaskPtr task) {
    double request_size = calc_delta_fair(base_slice_, task->weight());
    task->set_deadline(task->vruntime() + request_size);
}

void EEVDFScheduler::submit_task(TaskPtr task) {
    task->set_vruntime(min_vruntime_);
    update_deadline(task);
    ready_queue_.push_back(task);
}

TaskPtr EEVDFScheduler::select_next(int core_id) {
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    TaskPtr selected = nullptr;
    
    for (const auto& task : ready_queue_) {
        if (is_eligible(task)) {
            if (!selected || task->deadline() < selected->deadline()) {
                selected = task;
            }
        }
    }
    
    if (!selected) {
        auto min_it = std::min_element(
            ready_queue_.begin(), ready_queue_.end(),
            [](const auto& a, const auto& b) {
                return a->vruntime() < b->vruntime();
            });
        
        min_vruntime_ = (*min_it)->vruntime();
        
        for (const auto& task : ready_queue_) {
            if (is_eligible(task)) {
                if (!selected || task->deadline() < selected->deadline()) {
                    selected = task;
                }
            }
        }
    }
    
    if (!selected) {
        return nullptr;
    }
    
    ready_queue_.erase(
        std::remove(ready_queue_.begin(), ready_queue_.end(), selected),
        ready_queue_.end());
    
    context_switches_++;
    
    return selected;
}

void EEVDFScheduler::task_tick(TaskPtr task, double elapsed, int core_id) {
    double new_vruntime = task->vruntime() + calc_delta_fair(elapsed, task->weight());
    task->set_vruntime(new_vruntime);
    
    if (!ready_queue_.empty()) {
        auto min_it = std::min_element(
            ready_queue_.begin(), ready_queue_.end(),
            [](const auto& a, const auto& b) {
                return a->vruntime() < b->vruntime();
            });
        double queue_min = (*min_it)->vruntime();
        min_vruntime_ = std::min(new_vruntime, queue_min);
    } else {
        min_vruntime_ = new_vruntime;
    }
    
    if (!task->is_completed()) {
        update_deadline(task);
        ready_queue_.push_back(task);
        preemptions_++;
    }
}

void EEVDFScheduler::task_completed(TaskPtr task) {
    // Nothing special
}

} // namespace sched_sim