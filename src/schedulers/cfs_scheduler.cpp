#include "schedulers/cfs_scheduler.hpp"
#include <algorithm>

namespace sched_sim {

CFSScheduler::CFSScheduler(double min_granularity, double latency)
    : min_granularity_(min_granularity)
    , latency_(latency)
{
}

double CFSScheduler::calc_delta_fair(double delta, uint32_t weight) noexcept {
    return delta * NICE_0_LOAD / static_cast<double>(weight);
}

void CFSScheduler::submit_task(TaskPtr task) {
    task->set_vruntime(min_vruntime_);
    ready_queue_.insert(task);
}

TaskPtr CFSScheduler::select_next(int core_id) {
    if (ready_queue_.empty()) {
        return nullptr;
    }
    
    auto it = ready_queue_.begin();
    auto selected = *it;
    ready_queue_.erase(it);
    
    context_switches_++;
    
    return selected;
}

void CFSScheduler::task_tick(TaskPtr task, double elapsed, int core_id) {
    double new_vruntime = task->vruntime() + calc_delta_fair(elapsed, task->weight());
    task->set_vruntime(new_vruntime);
    
    if (!ready_queue_.empty()) {
        double queue_min = (*ready_queue_.begin())->vruntime();
        min_vruntime_ = std::min(new_vruntime, queue_min);
    } else {
        min_vruntime_ = new_vruntime;
    }
    
    if (!task->is_completed()) {
        ready_queue_.insert(task);
        preemptions_++;
    }
}

void CFSScheduler::task_completed(TaskPtr task) {
    // Nothing special
}

} // namespace sched_sim