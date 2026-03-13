#pragma once

#include "process.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdint>

struct SimulationMetrics {
    // Per-process stats (indexed by pid order)
    std::vector<int64_t> response_times_us;
    std::vector<int64_t> turnaround_times_us;
    std::vector<int64_t> wait_times_us;
    std::vector<int64_t> cpu_times_us;

    // Global stats
    int64_t total_simulation_time_us = 0;
    int total_processes = 0;
    int completed_processes = 0;
    int64_t total_context_switches = 0;
    int64_t total_idle_time_us = 0;

    // Compute throughput: completed tasks per second
    double throughput() const {
        if (total_simulation_time_us == 0) return 0;
        return static_cast<double>(completed_processes) /
               (static_cast<double>(total_simulation_time_us) / 1e6);
    }

    // CPU utilization
    double cpu_utilization() const {
        if (total_simulation_time_us == 0) return 0;
        return 1.0 - static_cast<double>(total_idle_time_us) /
                      static_cast<double>(total_simulation_time_us);
    }

    // Average response time
    double avg_response_time_us() const {
        if (response_times_us.empty()) return 0;
        return static_cast<double>(
            std::accumulate(response_times_us.begin(), response_times_us.end(), int64_t(0))
        ) / response_times_us.size();
    }

    // Average turnaround time
    double avg_turnaround_time_us() const {
        if (turnaround_times_us.empty()) return 0;
        return static_cast<double>(
            std::accumulate(turnaround_times_us.begin(), turnaround_times_us.end(), int64_t(0))
        ) / turnaround_times_us.size();
    }

    // Average wait time
    double avg_wait_time_us() const {
        if (wait_times_us.empty()) return 0;
        return static_cast<double>(
            std::accumulate(wait_times_us.begin(), wait_times_us.end(), int64_t(0))
        ) / wait_times_us.size();
    }

    // Max wait time (starvation indicator)
    int64_t max_wait_time_us() const {
        if (wait_times_us.empty()) return 0;
        return *std::max_element(wait_times_us.begin(), wait_times_us.end());
    }

    // Percentile response time (p = 0.95 for p95, 0.99 for p99)
    int64_t percentile_response_time_us(double p) const {
        if (response_times_us.empty()) return 0;
        auto sorted = response_times_us;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    }

    // Jain's fairness index on CPU share ratios
    // Each process's share = cpu_time / total_cpu_time_all_processes
    // Expected share = weight_i / sum_of_weights
    // Fairness index = (sum(share_i/expected_i))^2 / (n * sum((share_i/expected_i)^2))
    double jains_fairness_index(const std::vector<Process>& processes) const {
        if (processes.empty() || total_simulation_time_us == 0) return 0;

        // Compute total weight of completed processes
        double total_weight = 0;
        std::vector<double> ratios;

        for (const auto& p : processes) {
            if (p.completion_time_us < 0) continue;
            total_weight += p.weight;
        }
        if (total_weight == 0) return 0;

        double total_cpu = 0;
        for (const auto& p : processes) {
            if (p.completion_time_us < 0) continue;
            total_cpu += p.total_cpu_time_us;
        }
        if (total_cpu == 0) return 0;

        for (const auto& p : processes) {
            if (p.completion_time_us < 0) continue;
            double actual_share = static_cast<double>(p.total_cpu_time_us) / total_cpu;
            double expected_share = static_cast<double>(p.weight) / total_weight;
            if (expected_share > 0) {
                ratios.push_back(actual_share / expected_share);
            }
        }

        if (ratios.empty()) return 0;

        double sum_r = std::accumulate(ratios.begin(), ratios.end(), 0.0);
        double sum_r2 = 0;
        for (double r : ratios) sum_r2 += r * r;

        double n = static_cast<double>(ratios.size());
        return (sum_r * sum_r) / (n * sum_r2);
    }

    // Context switch rate (switches per second of sim time)
    double context_switch_rate() const {
        if (total_simulation_time_us == 0) return 0;
        return static_cast<double>(total_context_switches) /
               (static_cast<double>(total_simulation_time_us) / 1e6);
    }
};
