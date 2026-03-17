#include "schedulers/stride_scheduler.hpp"
#include <algorithm>

namespace sched_sim {

void StrideScheduler::submit_task(TaskPtr task) {
    uint32_t weight = task->weight();
    if (weight == 0) weight = 1;
    
    task->set_stride(STRIDE_CONSTANT / weight);
    
    if (!ready_queue_.empty()) {
        auto min_it = std::min_element(
            ready_queue_.begin(), ready_queue_.end(),
            [](const auto& a, const auto& b) {
                return a->pass_value() < b->pass_value();
            });
        task->set_pass_value((*min_it)->pass_value());
    } else {
        task->set_pass_value(0);
    }
    
    ready_queue_.push_back(task);
}

TaskPtr StrideScheduler::select_next(int core_id) {
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    auto min_it = std::min_element(
        ready_queue_.begin(), ready_queue_.end(),
        [](const auto& a, const auto& b) {
            return a->pass_value() < b->pass_value();
        });
    
    auto selected = *min_it;
    ready_queue_.erase(min_it);
    
    context_switches_++;
    
    return selected;
}

void StrideScheduler::task_tick(TaskPtr task, double elapsed, int core_id) {
    task->set_pass_value(task->pass_value() + task->stride());
    
    if (!task->is_completed()) {
        ready_queue_.push_back(task);
        preemptions_++;
    }
}

void StrideScheduler::task_completed(TaskPtr task) {
    // Nothing special
}

} // namespace sched_sim