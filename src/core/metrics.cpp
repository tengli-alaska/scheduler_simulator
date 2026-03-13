#include "scheduler/metrics.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace sched_sim {

void Metrics::calculate(const std::vector<TaskPtr>& completed_tasks, double total_time) {
    if (completed_tasks.empty()) {
        return;
    }
    
    num_completed = completed_tasks.size();
    
    // Collect response times
    std::vector<double> response_times;
    std::vector<double> wait_times;
    std::vector<double> turnaround_times;
    double total_exec_time = 0.0;
    
    for (const auto& task : completed_tasks) {
        response_times.push_back(task->response_time());
        wait_times.push_back(task->wait_time());
        turnaround_times.push_back(task->turnaround_time());
        total_exec_time += task->execution_time();
    }
    
    // Calculate means
    mean_response_time = std::accumulate(response_times.begin(), response_times.end(), 0.0) 
                        / num_completed;
    mean_wait_time = std::accumulate(wait_times.begin(), wait_times.end(), 0.0) 
                    / num_completed;
    mean_turnaround_time = std::accumulate(turnaround_times.begin(), turnaround_times.end(), 0.0) 
                          / num_completed;
    
    // Calculate percentiles
    std::sort(response_times.begin(), response_times.end());
    median_response_time = response_times[num_completed / 2];
    p95_response_time = response_times[static_cast<size_t>(num_completed * 0.95)];
    p99_response_time = response_times[static_cast<size_t>(num_completed * 0.99)];
    
    // Throughput and utilization
    throughput = num_completed / total_time;
    utilization = total_exec_time / total_time;
    
    // Fairness (coefficient of variation)
    double variance = 0.0;
    for (double tt : turnaround_times) {
        double diff = tt - mean_turnaround_time;
        variance += diff * diff;
    }
    variance /= num_completed;
    double stddev = std::sqrt(variance);
    fairness_cv = stddev / mean_turnaround_time;
}

void Metrics::print(const std::string& scheduler_name, 
                   const std::string& workload_name) const {
    std::cout << "\n========================================\n";
    std::cout << "Scheduler: " << scheduler_name << "\n";
    std::cout << "Workload:  " << workload_name << "\n";
    std::cout << "========================================\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Completed tasks:      " << num_completed << "\n";
    std::cout << "Mean response time:   " << mean_response_time << " ms\n";
    std::cout << "Median response time: " << median_response_time << " ms\n";
    std::cout << "P95 response time:    " << p95_response_time << " ms\n";
    std::cout << "P99 response time:    " << p99_response_time << " ms\n";
    std::cout << "Mean turnaround time: " << mean_turnaround_time << " ms\n";
    std::cout << "Mean wait time:       " << mean_wait_time << " ms\n";
    std::cout << std::setprecision(3);
    std::cout << "Throughput:           " << throughput << " tasks/s\n";
    std::cout << "Utilization:          " << (utilization * 100) << " %\n";
    std::cout << std::setprecision(4);
    std::cout << "Fairness (CV):        " << fairness_cv << "\n";
    std::cout << "Context switches:     " << context_switches << "\n";
    std::cout << "Preemptions:          " << preemptions << "\n";
    std::cout << "========================================\n";
}

void Metrics::to_csv(const std::string& scheduler_name,
                    const std::string& workload_name,
                    std::ostream& out) const {
    out << scheduler_name << ","
        << workload_name << ","
        << num_completed << ","
        << mean_response_time << ","
        << median_response_time << ","
        << p95_response_time << ","
        << p99_response_time << ","
        << mean_turnaround_time << ","
        << mean_wait_time << ","
        << throughput << ","
        << fairness_cv << ","
        << context_switches << ","
        << preemptions << "\n";
}

} // namespace sched_sim