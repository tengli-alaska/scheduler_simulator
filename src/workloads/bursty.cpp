#include "scheduler/workload.hpp"

extern "C" {
    #include "rngs.h"
    #include "rvgs.h"
}

namespace sched_sim {

std::vector<TaskPtr> BurstyWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);
    
    double arrival_time = 0.0;
    uint32_t burst_size = 20;
    uint32_t num_bursts = (num_tasks + burst_size - 1) / burst_size;
    uint32_t task_count = 0;
    
    SelectStream(0);
    
    for (uint32_t burst = 0; burst < num_bursts && task_count < num_tasks; ++burst) {
        arrival_time += Uniform(20.0, 40.0);
        
        for (uint32_t i = 0; i < burst_size && task_count < num_tasks; ++i) {
            arrival_time += Uniform(0.1, 0.5);
            
            SelectStream(1);
            double exec_time = Uniform(5.0, 20.0);
            
            SelectStream(2);
            double r = Random();
            int nice = (r < 0.33) ? -5 : (r < 0.67) ? 0 : 5;
            
            tasks.push_back(std::make_shared<Task>(task_count, arrival_time, exec_time, nice));
            
            task_count++;
            SelectStream(0);
        }
    }
    
    return tasks;
}

} // namespace sched_sim