#include "scheduler/workload.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sched_sim {

namespace {

std::vector<std::string> split_csv_row(const std::string& line) {
    // Simple splitter is sufficient for current trace files (no quoted commas).
    std::vector<std::string> cells;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        while (!item.empty() && (item.back() == '\r' || item.back() == '\n')) {
            item.pop_back();
        }
        cells.push_back(item);
    }
    return cells;
}

std::string trim_copy(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

bool safe_to_double(const std::string& s, double& out) {
    try {
        size_t idx = 0;
        out = std::stod(s, &idx);
        return idx > 0;
    } catch (...) {
        return false;
    }
}

bool safe_to_int(const std::string& s, int& out) {
    try {
        size_t idx = 0;
        out = std::stoi(s, &idx);
        return idx > 0;
    } catch (...) {
        return false;
    }
}

int clamp_nice(int v) {
    if (v < -20) return -20;
    if (v > 19) return 19;
    return v;
}

} // namespace

TraceReplayWorkload::TraceReplayWorkload(TraceType trace_type, std::string csv_path)
    : trace_type_(trace_type), csv_path_(std::move(csv_path)) {
    if (!csv_path_.empty()) return;

    if (trace_type_ == TraceType::GoogleV3) {
        csv_path_ = "real-time-workloads/google_v3/google_v3_workload.csv";
    } else {
        csv_path_ = "real-time-workloads/alibaba_v2018/batch_instance_subset_head_40000_with_header.csv";
    }
}

std::string TraceReplayWorkload::name() const {
    if (trace_type_ == TraceType::GoogleV3) return "GoogleTraceV3";
    return "AlibabaTraceV2018";
}

std::string TraceReplayWorkload::description() const {
    if (trace_type_ == TraceType::GoogleV3) {
        return "Google Borg V3 trace replay (arrival/cpu burst from extracted CSV)";
    }
    return "Alibaba Cluster Trace v2018 replay (subset CSV, start/end as task timing)";
}

std::vector<TaskPtr> TraceReplayWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);

    std::ifstream in(csv_path_);
    if (!in.is_open()) {
        std::cerr << "Error: could not open trace file: " << csv_path_ << "\n";
        return tasks;
    }

    std::string line;
    std::vector<std::string> header;
    bool has_header = false;

    if (std::getline(in, line)) {
        header = split_csv_row(line);
        // Header detection by known first columns.
        if (!header.empty()) {
            std::string first = trim_copy(header[0]);
            has_header = (first == "arrival_time_us" || first == "instance_name");
        }
        if (!has_header) {
            // rewind handling by processing this row as data
            in.clear();
            in.seekg(0);
        }
    }

    std::vector<std::tuple<double, double, int>> raw;  // arrival_ms, exec_ms, nice
    raw.reserve(num_tasks);

    if (trace_type_ == TraceType::GoogleV3) {
        int idx_arrival = 0;
        int idx_cpu = 1;
        int idx_nice = 3;

        if (has_header) {
            for (int i = 0; i < static_cast<int>(header.size()); ++i) {
                const std::string h = trim_copy(header[i]);
                if (h == "arrival_time_us") idx_arrival = i;
                else if (h == "cpu_burst_duration_us") idx_cpu = i;
                else if (h == "nice" || h == "nice_value") idx_nice = i;
            }
        }

        while (raw.size() < num_tasks && std::getline(in, line)) {
            if (line.empty()) continue;
            auto cells = split_csv_row(line);
            if (cells.size() <= static_cast<size_t>(std::max({idx_arrival, idx_cpu, idx_nice}))) {
                continue;
            }

            double arrival_us = 0.0;
            double cpu_us = 0.0;
            int nice = 0;
            if (!safe_to_double(cells[idx_arrival], arrival_us)) continue;
            if (!safe_to_double(cells[idx_cpu], cpu_us)) continue;
            if (!safe_to_int(cells[idx_nice], nice)) nice = 0;

            double arrival_ms = arrival_us / 1000.0;
            double exec_ms = std::max(0.1, cpu_us / 1000.0);
            raw.emplace_back(arrival_ms, exec_ms, clamp_nice(nice));
        }
    } else {
        // Alibaba batch_instance format:
        // ... task_type,status,start_time,end_time,...
        int idx_task_type = 3;
        int idx_status = 4;
        int idx_start = 5;
        int idx_end = 6;

        if (has_header) {
            for (int i = 0; i < static_cast<int>(header.size()); ++i) {
                const std::string h = trim_copy(header[i]);
                if (h == "task_type") idx_task_type = i;
                else if (h == "status") idx_status = i;
                else if (h == "start_time") idx_start = i;
                else if (h == "end_time") idx_end = i;
            }
        }

        while (raw.size() < num_tasks && std::getline(in, line)) {
            if (line.empty()) continue;
            auto cells = split_csv_row(line);
            if (cells.size() <= static_cast<size_t>(std::max({idx_task_type, idx_status, idx_start, idx_end}))) {
                continue;
            }

            const std::string status = trim_copy(cells[idx_status]);
            if (!status.empty() && status != "Terminated" && status != "Finished" && status != "Completed") {
                continue;
            }

            double start_t = 0.0;
            double end_t = 0.0;
            int task_type = 1;
            if (!safe_to_double(cells[idx_start], start_t)) continue;
            if (!safe_to_double(cells[idx_end], end_t)) continue;
            if (!safe_to_int(cells[idx_task_type], task_type)) task_type = 1;
            if (end_t <= start_t) continue;

            // Conservative mapping: lower task_type => higher priority.
            int nice = clamp_nice(task_type - 5);
            double arrival_ms = start_t;
            double exec_ms = std::max(0.1, end_t - start_t);
            raw.emplace_back(arrival_ms, exec_ms, nice);
        }
    }

    if (raw.empty()) {
        std::cerr << "Warning: no valid rows parsed from trace file: " << csv_path_ << "\n";
        return tasks;
    }

    std::sort(raw.begin(), raw.end(),
              [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });

    // Normalize to t=0 for stable simulation stop-time behavior.
    const double t0 = std::get<0>(raw.front());
    for (size_t i = 0; i < raw.size(); ++i) {
        double arrival_ms = std::max(0.0, std::get<0>(raw[i]) - t0);
        double exec_ms = std::get<1>(raw[i]);
        int nice = std::get<2>(raw[i]);
        tasks.push_back(std::make_shared<Task>(static_cast<uint32_t>(i), arrival_ms, exec_ms, nice));
    }

    std::cout << "Loaded " << tasks.size() << " tasks from trace: " << csv_path_ << "\n";
    return tasks;
}

} // namespace sched_sim
