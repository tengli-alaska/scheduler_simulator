/**
 * @file simulator.hpp
 * @brief Main discrete-event simulator
 */

#pragma once

#include "task.hpp"
#include "scheduler.hpp"
#include "event.hpp"
#include <vector>
#include <memory>

namespace sched_sim {

/**
 * @class Simulator
 * @brief Main simulation engine for scheduler evaluation
 */
class Simulator {
public:
    /**
     * @brief Construct a new Simulator
     * @param scheduler Unique pointer to scheduler instance
     * @param num_cores Number of CPU cores to simulate
     * @param time_slice Time slice duration (milliseconds)
     */
    Simulator(SchedulerPtr scheduler, 
              int num_cores = 1, 
              double time_slice = 5.0);
    
    /**
     * @brief Add a task to the simulation
     * @param task Shared pointer to the task
     */
    void add_task(TaskPtr task);
    
    /**
     * @brief Run the simulation until stop_time
     * @param stop_time Time to stop simulation
     */
    void run(double stop_time);
    
    // Getters
    const std::vector<TaskPtr>& completed_tasks() const noexcept {
        return completed_tasks_;
    }
    
    double current_time() const noexcept { return current_time_; }
    int num_cores() const noexcept { return num_cores_; }
    double time_slice() const noexcept { return time_slice_; }
    
    const Scheduler* scheduler() const noexcept { return scheduler_.get(); }
    
private:
    void handle_arrival(const Event& evt);
    void handle_time_slice(const Event& evt);
    void schedule_task_on_core(int core_id);
    
    SchedulerPtr scheduler_;
    EventQueue events_;
    
    int num_cores_;
    double time_slice_;
    double current_time_ = 0.0;
    double stop_time_ = 0.0;
    
    std::vector<TaskPtr> running_tasks_;      // Task per core
    std::vector<double> last_event_time_;     // Last event time per core
    std::vector<TaskPtr> all_tasks_;
    std::vector<TaskPtr> completed_tasks_;
};

} // namespace sched_sim