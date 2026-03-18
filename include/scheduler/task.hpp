/**
 * @file task.hpp
 * @brief Task class representing a schedulable unit of work
 */

#pragma once

#include <cstdint>
#include <memory>
#include <array>

namespace sched_sim {

// Nice value to weight mapping (Linux kernel)
constexpr std::array<uint32_t, 40> NICE_TO_WEIGHT = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */  9548,  7620,  6100,  4904,  3906,
    /*  -5 */  3121,  2501,  1991,  1586,  1277,
    /*   0 */  1024,   820,   655,   526,   423,
    /*   5 */   335,   272,   215,   172,   137,
    /*  10 */   110,    87,    70,    56,    45,
    /*  15 */    36,    29,    23,    18,    15,
};

constexpr uint32_t NICE_0_LOAD = 1024;

/**
 * @class Task
 * @brief Represents a schedulable task with timing and priority information
 */
class Task {
public:
    /**
     * @brief Construct a new Task object
     * @param id Unique task identifier
     * @param arrival_time Time when task arrives in system
     * @param execution_time Total execution time required
     * @param nice Nice value (-20 to +19, lower = higher priority)
     */
    Task(uint32_t id, double arrival_time, double execution_time, int nice = 0);
    
    // Prevent copying (tasks should be unique)
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    // Allow moving
    Task(Task&&) = default;
    Task& operator=(Task&&) = default;
    
    ~Task() = default;
    
    // Basic getters
    uint32_t id() const noexcept { return id_; }
    double arrival_time() const noexcept { return arrival_time_; }
    double execution_time() const noexcept { return execution_time_; }
    double remaining_time() const noexcept { return remaining_time_; }
    int nice() const noexcept { return nice_; }
    uint32_t weight() const noexcept { return weight_; }
    
    // Scheduling state (getters)
    double vruntime() const noexcept { return vruntime_; }
    double deadline() const noexcept { return deadline_; }
    double eligible_time() const noexcept { return eligible_time_; }
    uint32_t stride() const noexcept { return stride_; }
    uint32_t pass_value() const noexcept { return pass_value_; }
    int current_queue() const noexcept { return current_queue_; }
    double allotment_remaining() const noexcept { return allotment_remaining_; }

    // Scheduling state (setters)
    void set_vruntime(double v) noexcept { vruntime_ = v; }
    void set_deadline(double d) noexcept { deadline_ = d; }
    void set_eligible_time(double e) noexcept { eligible_time_ = e; }
    void set_stride(uint32_t s) noexcept { stride_ = s; }
    void set_pass_value(uint32_t p) noexcept { pass_value_ = p; }
    void set_current_queue(int q) noexcept { current_queue_ = q; }
    void set_allotment_remaining(double a) noexcept { allotment_remaining_ = a; }
    
    // Execution tracking
    void start(double time);
    void complete(double time);
    void execute(double duration);
    bool is_completed() const noexcept;
    
    // Statistics
    double start_time() const noexcept { return start_time_; }
    double completion_time() const noexcept { return completion_time_; }
    double response_time() const noexcept;
    double wait_time() const noexcept { return wait_time_; }
    double turnaround_time() const noexcept;
    uint32_t preemption_count() const noexcept { return preemption_count_; }
    
    void increment_preemptions() noexcept { preemption_count_++; }
    void reset();
    
private:
    // Identity and timing
    uint32_t id_;
    double arrival_time_;
    double execution_time_;
    double remaining_time_;
    
    // Priority
    int nice_;
    uint32_t weight_;
    
    // Scheduler-specific state
    double vruntime_ = 0.0;
    double deadline_ = 0.0;
    double eligible_time_ = 0.0;
    uint32_t stride_ = 0;
    uint32_t pass_value_ = 0;
    int current_queue_ = 0;
    double allotment_remaining_ = 0.0;
    
    // Execution tracking
    double start_time_ = -1.0;
    double completion_time_ = -1.0;
    double wait_time_ = 0.0;
    uint32_t preemption_count_ = 0;
};

using TaskPtr = std::shared_ptr<Task>;

} // namespace sched_sim