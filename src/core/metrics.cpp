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
    p95_response_time = response_times[static_cast<size_t>(num_completed * 0.95)];
    p99_response_time = response_times[static_cast<size_t>(num_completed * 0.99)];
    
    // Throughput and utilization
    throughput = num_completed / total_time;
    utilization = total_exec_time / total_time;
    
    // Jain's Fairness Index: J = (sum(x_i))^2 / (n * sum(x_i^2))
    // where x_i = execution_time / turnaround_time (normalized throughput per task)
    double sum_x = 0.0;
    double sum_x2 = 0.0;
    for (size_t i = 0; i < num_completed; ++i) {
        double x = completed_tasks[i]->execution_time() / turnaround_times[i];
        sum_x += x;
        sum_x2 += x * x;
    }
    jains_fairness = (sum_x * sum_x) / (num_completed * sum_x2);
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
    std::cout << "P95 response time:    " << p95_response_time << " ms\n";
    std::cout << "P99 response time:    " << p99_response_time << " ms\n";
    std::cout << "Mean turnaround time: " << mean_turnaround_time << " ms\n";
    std::cout << "Mean wait time:       " << mean_wait_time << " ms\n";
    std::cout << std::setprecision(3);
    std::cout << "Throughput:           " << throughput << " tasks/s\n";
    std::cout << "Utilization:          " << (utilization * 100) << " %\n";
    std::cout << std::setprecision(4);
    std::cout << "Jain's Fairness:      " << jains_fairness << "\n";
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
        << p95_response_time << ","
        << p99_response_time << ","
        << mean_turnaround_time << ","
        << mean_wait_time << ","
        << throughput << ","
        << jains_fairness << ","
        << context_switches << ","
        << preemptions << "\n";
}

} // namespace sched_sim