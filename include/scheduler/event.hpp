/**
 * @file event.hpp
 * @brief Event system for discrete-event simulation
 */

#pragma once

#include "task.hpp"
#include <queue>
#include <vector>
#include <functional>
#include <optional>

namespace sched_sim {

/**
 * @enum EventType
 * @brief Types of events in the simulation
 */
enum class EventType {
    ARRIVAL,         ///< Task arrival event
    TIME_SLICE,      ///< Time slice expiration
    COMPLETION,      ///< Task completion (deprecated, handled in TIME_SLICE)
    PRIORITY_BOOST   ///< Priority boost event (for MLFQ)
};

/**
 * @struct Event
 * @brief Represents a discrete event in the simulation
 */
struct Event {
    EventType type;
    double time;
    TaskPtr task;
    int core_id;
    
    /**
     * @brief Comparison operator for priority queue (min-heap)
     */
    bool operator>(const Event& other) const noexcept {
        return time > other.time;
    }
};

/**
 * @class EventQueue
 * @brief Priority queue for managing simulation events
 */
class EventQueue {
public:
    /**
     * @brief Schedule a new event
     */
    void schedule(EventType type, double time, TaskPtr task, int core_id = 0);
    
    /**
     * @brief Get the next event (earliest time)
     * @return Next event, or std::nullopt if queue is empty
     */
    std::optional<Event> get_next();
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const noexcept { return queue_.empty(); }
    
    /**
     * @brief Get number of events in queue
     */
    size_t size() const noexcept { return queue_.size(); }
    
    /**
     * @brief Remove all events for a specific task
     */
    void cancel_task(const TaskPtr& task);
    
private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> queue_;
};

} // namespace sched_sim
