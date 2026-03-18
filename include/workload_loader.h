#pragma once

#include "process.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <map>

// CFS weight table: nice -> weight
inline int nice_to_weight(int nice) {
    static const std::map<int, int> weights = {
        {-20, 88761}, {-19, 71755}, {-18, 56483}, {-17, 46273}, {-16, 36291},
        {-15, 29154}, {-14, 23254}, {-13, 18705}, {-12, 14949}, {-11, 11916},
        {-10,  9548}, { -9,  7620}, { -8,  6100}, { -7,  4904}, { -6,  3906},
        { -5,  3121}, { -4,  2501}, { -3,  1991}, { -2,  1586}, { -1,  1277},
        {  0,  1024}, {  1,   820}, {  2,   655}, {  3,   526}, {  4,   423},
        {  5,   335}, {  6,   272}, {  7,   215}, {  8,   172}, {  9,   137},
        { 10,   110}, { 11,    87}, { 12,    70}, { 13,    56}, { 14,    45},
        { 15,    36}, { 16,    29}, { 17,    23}, { 18,    18}, { 19,    15},
    };
    auto it = weights.find(nice);
    if (it != weights.end()) return it->second;
    return 1024; // default
}

// Load workload from CSV produced by extract_google_v3.py or synthetic generators.
// Expected CSV header:
//   arrival_time_us,cpu_burst_duration_us,io_burst_duration_us,nice,weight,scheduling_class,cpu_request
inline std::vector<Process> load_workload_csv(const std::string& path, int max_processes = 0) {
    std::vector<Process> processes;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open workload file: " << path << std::endl;
        return processes;
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    int pid = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string token;

        Process p;
        p.pid = pid++;

        // arrival_time_us
        std::getline(ss, token, ',');
        p.arrival_time_us = std::stoll(token);

        // cpu_burst_duration_us
        std::getline(ss, token, ',');
        int64_t cpu_burst = std::stoll(token);
        p.cpu_bursts_us.push_back(cpu_burst);
        p.remaining_burst_us = cpu_burst;

        // io_burst_duration_us
        std::getline(ss, token, ',');
        int64_t io_burst = std::stoll(token);
        if (io_burst > 0) {
            p.io_bursts_us.push_back(io_burst);
        }

        // nice
        std::getline(ss, token, ',');
        p.nice = std::stoi(token);

        // weight
        std::getline(ss, token, ',');
        p.weight = std::stoi(token);

        // scheduling_class
        std::getline(ss, token, ',');
        p.scheduling_class = std::stoi(token);

        // cpu_request
        std::getline(ss, token, ',');
        p.cpu_request = std::stod(token);

        processes.push_back(p);

        if (max_processes > 0 && pid >= max_processes) break;
    }

    // Sort by arrival time
    std::sort(processes.begin(), processes.end(),
              [](const Process& a, const Process& b) {
                  return a.arrival_time_us < b.arrival_time_us;
              });

    std::cout << "Loaded " << processes.size() << " processes from " << path << std::endl;
    return processes;
}
