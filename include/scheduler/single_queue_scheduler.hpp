/**
 * @file single_queue_scheduler.hpp
 * @brief Base class for single-queue schedulers (CFS, EEVDF, Stride)
 *
 * Manages a single ordered collection of ready tasks.
 * Subclasses define the ordering by providing a comparator,
 * and implement schedule(), on_cpu_used(), should_preempt().
 */

#pragma once

#include "scheduler.hpp"
#include <vector>
#include <functional>
#include <algorithm>

namespace sched_sim {

class SingleQueueScheduler : public Scheduler {
public:
    /// Comparator type: returns true if a should be scheduled before b
    using Comparator = std::function<bool(const TaskPtr&, const TaskPtr&)>;

    explicit SingleQueueScheduler(Comparator cmp)
        : comparator_(std::move(cmp)) {}

    void add_task(TaskPtr task, double /*current_time*/) override {
        ready_queue_.push_back(task);
    }

    void remove_task(uint32_t task_id) override {
        ready_queue_.erase(
            std::remove_if(ready_queue_.begin(), ready_queue_.end(),
                           [task_id](const TaskPtr& t) { return t->id() == task_id; }),
            ready_queue_.end()
        );
    }

    int ready_count() const override {
        return static_cast<int>(ready_queue_.size());
    }

    void reset() override {
        ready_queue_.clear();
    }

protected:
    /// Find the task that should run next according to the comparator
    TaskPtr find_best() const {
        if (ready_queue_.empty()) return nullptr;
        return *std::min_element(ready_queue_.begin(), ready_queue_.end(), comparator_);
    }

    /// Access the ready queue for subclass-specific operations
    std::vector<TaskPtr>& ready_queue() { return ready_queue_; }
    const std::vector<TaskPtr>& ready_queue() const { return ready_queue_; }

    Comparator comparator_;

private:
    std::vector<TaskPtr> ready_queue_;
};

} // namespace sched_sim
