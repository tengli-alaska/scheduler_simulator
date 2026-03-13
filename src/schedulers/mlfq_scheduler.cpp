/**
 * @file mlfq_scheduler.cpp
 * @brief Multi-Level Feedback Queue implementation
 */

#include "schedulers/mlfq_scheduler.hpp"
#include <iostream>

namespace sched_sim {

MLFQScheduler::MLFQScheduler() {
    // Initialize time quantums - increasing at lower priorities
    time_quantums_[0] = 2.0;   // Queue 0 (highest): 2ms
    time_quantums_[1] = 4.0;   // Queue 1 (medium):  4ms
    time_quantums_[2] = 8.0;   // Queue 2 (lowest):  8ms
}

void MLFQScheduler::priority_boost() {
    boosts_++;
    
    // Move all tasks from lower queues to highest priority queue
    for (int i = 1; i < NUM_QUEUES; ++i) {
        while (!queues_[i].empty()) {
            auto task = queues_[i].front();
            queues_[i].pop_front();  // Now this works!
            
            task->set_current_queue(0);
            queues_[0].push_back(task);
            promotions_++;
        }
    }
}

void MLFQScheduler::submit_task(TaskPtr task) {
    // New tasks always start at highest priority (queue 0)
    task->set_current_queue(0);
    queues_[0].push_back(task);
}

TaskPtr MLFQScheduler::select_next(int core_id) {
    // Select from highest priority non-empty queue
    for (int i = 0; i < NUM_QUEUES; ++i) {
        if (!queues_[i].empty()) {
            auto task = queues_[i].front();
            queues_[i].pop_front();  // Now this works!
            return task;
        }
    }
    
    return nullptr;
}

void MLFQScheduler::task_tick(TaskPtr task, double elapsed, int core_id) {
    int queue = task->current_queue();
    double quantum = time_quantums_[queue];
    
    // Check if task used full quantum
    bool used_full_quantum = (elapsed >= quantum - 0.001);
    
    if (!task->is_completed()) {
        // Demote if used full quantum (CPU-bound behavior)
        if (used_full_quantum && queue < NUM_QUEUES - 1) {
            int new_queue = queue + 1;
            task->set_current_queue(new_queue);
            demotions_++;
        }
        // else: keep current priority (I/O-bound behavior)
        
        // Requeue at current (possibly demoted) priority
        queues_[task->current_queue()].push_back(task);
        preemptions_++;
    }
}

void MLFQScheduler::task_completed(TaskPtr task) {
    // Nothing special needed
    
    // Note: In a full implementation, we could check if it's time
    // for priority boost here based on simulation time
}

} // namespace sched_sim