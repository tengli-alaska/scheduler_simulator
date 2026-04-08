/**
 * @file metrics.hpp
 * @brief Performance metrics calculation and reporting
 */

#pragma once

#include "task.hpp"
#include <vector>
#include <string>
#include <iosfwd>

namespace sched_sim {

/**
 * @struct Metrics
 * @brief Collection of performance metrics
 */
struct Metrics {
    uint32_t num_completed = 0;
    double mean_response_time = 0.0;
    double p95_response_time = 0.0;
    double p99_response_time = 0.0;
    double mean_turnaround_time = 0.0;
    double mean_wait_time = 0.0;
    double throughput = 0.0;
    double utilization = 0.0;
    double jains_fairness = 0.0;  // Jain's Fairness Index [0, 1]
    uint32_t context_switches = 0;
    uint32_t preemptions = 0;
    
    /**
     * @brief Calculate metrics from completed tasks
     * @param completed_tasks Vector of completed tasks
     * @param total_time Total simulation time
     * @param num_cores Number of CPU cores (default 1)
     */
    void calculate(const std::vector<TaskPtr>& completed_tasks, double total_time, uint32_t num_cores = 1);
    
    /**
     * @brief Print metrics to console
     */
    void print(const std::string& scheduler_name, 
              const std::string& workload_name) const;
    
    /**
     * @brief Write metrics to CSV stream
     */
    void to_csv(const std::string& scheduler_name,
               const std::string& workload_name,
               std::ostream& out) const;
};

} // namespace sched_sim