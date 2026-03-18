#pragma once

#include "scheduler.h"
#include <set>
#include <functional>
#include <algorithm>

// Base class for single-queue schedulers (CFS, EEVDF, Stride).
// Manages a single ordered set of ready processes.
// Subclasses define the ordering by providing a comparator,
// and implement schedule(), on_cpu_used(), should_preempt().
class SingleQueueScheduler : public Scheduler {
public:
    // Comparator type: returns true if a should be scheduled before b
    using Comparator = std::function<bool(const Process*, const Process*)>;

    explicit SingleQueueScheduler(Comparator cmp)
        : comparator_(std::move(cmp)) {}

    void add_process(Process* proc, int64_t current_time_us) override {
        ready_queue_.push_back(proc);
    }

    void remove_process(int pid) override {
        ready_queue_.erase(
            std::remove_if(ready_queue_.begin(), ready_queue_.end(),
                           [pid](const Process* p) { return p->pid == pid; }),
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
    // Find the process that should run next according to the comparator
    Process* find_best() const {
        if (ready_queue_.empty()) return nullptr;
        return *std::min_element(ready_queue_.begin(), ready_queue_.end(), comparator_);
    }

    // Access the ready queue for subclass-specific operations
    std::vector<Process*>& ready_queue() { return ready_queue_; }
    const std::vector<Process*>& ready_queue() const { return ready_queue_; }

    Comparator comparator_;

private:
    std::vector<Process*> ready_queue_;
};
