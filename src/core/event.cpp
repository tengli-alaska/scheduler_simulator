#include "scheduler/event.hpp"

namespace sched_sim {

void EventQueue::schedule(EventType type, double time, TaskPtr task, int core_id) {
    queue_.push(Event{type, time, task, core_id});
}

std::optional<Event> EventQueue::get_next() {
    if (queue_.empty()) {
        return std::nullopt;
    }
    
    Event evt = queue_.top();
    queue_.pop();
    return evt;
}

void EventQueue::cancel_task(const TaskPtr& task) {
    // Note: std::priority_queue doesn't support efficient removal
    // In production, would use a more sophisticated data structure
    // For now, events will be filtered when processed
}

} // namespace sched_sim