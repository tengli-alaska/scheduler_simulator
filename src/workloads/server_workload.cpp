#include "scheduler/workload.hpp"

extern "C" {
    #include "rngs.h"
    #include "rvgs.h"
}

namespace sched_sim {

std::vector<TaskPtr> ServerWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);

    double arrival_time = 0.0;
    uint32_t task_count = 0;

    SelectStream(0);

    while (task_count < num_tasks) {
        // Decide if this is a burst period or steady period
        SelectStream(3);
        double burst_roll = Random();
        SelectStream(0);

        if (burst_roll < 0.3) {
            // === Burst period (traffic spike) ===
            // 8-15 requests arrive in rapid succession
            SelectStream(4);
            int burst_size = (int)Uniform(8.0, 15.0);
            SelectStream(0);

            for (int b = 0; b < burst_size && task_count < num_tasks; ++b) {
                arrival_time += Uniform(0.1, 0.5);  // very tight arrivals

                SelectStream(1);
                double r = Random();
                double exec_time;
                int nice;

                if (r < 0.60) {
                    // API request handling (short I/O-bound)
                    exec_time = Uniform(1.0, 8.0);
                    nice = -5;  // high priority, user-facing
                } else if (r < 0.85) {
                    // Database query or middleware processing
                    exec_time = Uniform(5.0, 20.0);
                    nice = 0;   // normal priority
                } else {
                    // Heavy compute: report generation, image processing
                    exec_time = Uniform(25.0, 60.0);
                    nice = 5;   // lower priority batch work
                }

                tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
                task_count++;
                SelectStream(0);
            }
        } else {
            // === Steady period (normal traffic) ===
            arrival_time += Exponential(2.0);

            SelectStream(1);
            double r = Random();
            double exec_time;
            int nice;

            if (r < 0.55) {
                // Standard API request
                exec_time = Uniform(2.0, 10.0);
                nice = -5;
            } else if (r < 0.80) {
                // Background job: logging, health checks, cron tasks
                exec_time = Uniform(8.0, 25.0);
                nice = 10;  // low priority background
            } else if (r < 0.95) {
                // Moderate compute: data aggregation
                exec_time = Uniform(15.0, 40.0);
                nice = 0;
            } else {
                // Rare heavy task: full report, batch ETL
                exec_time = Uniform(50.0, 100.0);
                nice = 5;
            }

            tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
            task_count++;
            SelectStream(0);
        }
    }

    return tasks;
}

} // namespace sched_sim
