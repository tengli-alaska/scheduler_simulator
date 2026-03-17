#include "scheduler/task.hpp"
#include <algorithm>

namespace sched_sim {

Task::Task(uint32_t id, double arrival_time, double execution_time, int nice)
    : id_(id)
    , arrival_time_(arrival_time)
    , execution_time_(execution_time)
    , remaining_time_(execution_time)
    , nice_(nice)
{
    // Convert nice to weight
    int nice_idx = std::clamp(nice + 20, 0, 39);
    weight_ = NICE_TO_WEIGHT[nice_idx];
}

void Task::start(double time) {
    if (start_time_ < 0) {
        start_time_ = time;
        wait_time_ = time - arrival_time_;
    }
}

void Task::complete(double time) {
    completion_time_ = time;
}

void Task::execute(double duration) {
    remaining_time_ -= duration;
    if (remaining_time_ < 0) {
        remaining_time_ = 0;
    }
}

bool Task::is_completed() const noexcept {
    return remaining_time_ <= 0.001;
}

double Task::response_time() const noexcept {
    if (start_time_ < 0) return 0.0;
    return start_time_ - arrival_time_;
}

double Task::turnaround_time() const noexcept {
    if (completion_time_ < 0) return 0.0;
    return completion_time_ - arrival_time_;
}

void Task::reset() {
    remaining_time_ = execution_time_;
    start_time_ = -1.0;
    completion_time_ = -1.0;
    vruntime_ = 0.0;
    deadline_ = 0.0;
    pass_value_ = 0;
    stride_ = 0;
    current_queue_ = 0;
    preemption_count_ = 0;
    wait_time_ = 0.0;
}

} // namespace sched_sim