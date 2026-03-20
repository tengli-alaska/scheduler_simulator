#include "scheduler/workload.hpp"

extern "C" {
    #include "rngs.h"
    #include "rvgs.h"
}

namespace sched_sim {

std::vector<TaskPtr> DesktopWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);

    double arrival_time = 0.0;
    uint32_t task_count = 0;

    SelectStream(0);

    while (task_count < num_tasks) {
        // Simulate user activity: periods of active use with occasional pauses
        SelectStream(3);
        double activity_roll = Random();
        SelectStream(0);

        if (activity_roll < 0.25) {
            // === User idle / thinking gap ===
            arrival_time += Uniform(10.0, 30.0);

            // Background task runs during idle (system update, indexing, backup)
            SelectStream(1);
            double exec_time = Uniform(30.0, 80.0);
            int nice = 10;  // low priority background

            tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
            task_count++;
            SelectStream(0);
        } else if (activity_roll < 0.55) {
            // === Rapid interactive burst (typing, clicking, tab switching) ===
            SelectStream(4);
            int burst_size = (int)Uniform(3.0, 8.0);
            SelectStream(0);

            for (int b = 0; b < burst_size && task_count < num_tasks; ++b) {
                arrival_time += Uniform(0.2, 1.0);  // fast interactions

                SelectStream(1);
                double exec_time = Uniform(0.5, 5.0);  // very short interactive tasks
                int nice = -5;  // high priority, user-facing

                tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
                task_count++;
                SelectStream(0);
            }
        } else if (activity_roll < 0.80) {
            // === Normal interactive task (opening file, running command) ===
            arrival_time += Exponential(2.0);

            SelectStream(1);
            double r = Random();
            double exec_time;
            int nice;

            if (r < 0.70) {
                // Quick shell command, file open, search
                exec_time = Uniform(1.0, 10.0);
                nice = -5;
            } else {
                // Moderate task: linting, formatting, small script
                exec_time = Uniform(8.0, 25.0);
                nice = 0;
            }

            tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
            task_count++;
            SelectStream(0);
        } else {
            // === Heavy batch job (compilation, rendering, test suite) ===
            arrival_time += Exponential(3.0);

            SelectStream(1);
            double r = Random();
            double exec_time;
            int nice;

            if (r < 0.50) {
                // Compilation or build
                exec_time = Uniform(40.0, 90.0);
                nice = 5;  // lower priority so it doesn't starve interactive
            } else if (r < 0.80) {
                // Test suite run
                exec_time = Uniform(20.0, 50.0);
                nice = 0;
            } else {
                // Very heavy: full rebuild, video render, ML training step
                exec_time = Uniform(80.0, 150.0);
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
