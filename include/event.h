#pragma once

#include <cstdint>
#include <queue>
#include <vector>

enum class EventType {
    PROCESS_ARRIVAL,    // A new process enters the ready queue
    CPU_BURST_COMPLETE, // Current CPU burst finished (natural completion)
    TIME_SLICE_EXPIRE,  // Preemption: time quantum exhausted
    IO_COMPLETE,        // IO burst finished, process returns to ready queue
    MLFQ_PRIORITY_BOOST // Periodic priority boost for MLFQ
};

struct Event {
    int64_t time_us;    // When this event fires
    EventType type;
    int pid;            // Which process this event concerns (-1 for global events)

    // Min-heap: earliest event first
    bool operator>(const Event& other) const {
        if (time_us != other.time_us) return time_us > other.time_us;
        // Tie-break: arrivals before completions before preemptions
        return static_cast<int>(type) > static_cast<int>(other.type);
    }
};

class EventQueue {
public:
    void push(const Event& e) {
        heap_.push(e);
    }

    Event pop() {
        Event e = heap_.top();
        heap_.pop();
        return e;
    }

    const Event& peek() const {
        return heap_.top();
    }

    bool empty() const {
        return heap_.empty();
    }

    size_t size() const {
        return heap_.size();
    }

    // Remove all pending events for a given pid and type
    // (used when preempting: cancel the pending CPU_BURST_COMPLETE)
    void cancel(int pid, EventType type) {
        std::priority_queue<Event, std::vector<Event>, std::greater<Event>> new_heap;
        while (!heap_.empty()) {
            Event e = heap_.top();
            heap_.pop();
            if (!(e.pid == pid && e.type == type)) {
                new_heap.push(e);
            }
        }
        heap_ = std::move(new_heap);
    }

private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> heap_;
};
