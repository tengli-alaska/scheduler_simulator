#include "scheduler/workload.hpp"

extern "C" {
    #include "rngs.h"
    #include "rvgs.h"
}

namespace sched_sim {

std::vector<TaskPtr> BimodalWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);
    
    double arrival_time = 0.0;
    
    SelectStream(0);
    
    for (uint32_t i = 0; i < num_tasks; ++i) {
        arrival_time += Exponential(1.5);
        
        SelectStream(1);
        double r = Random();
        double exec_time;
        int nice;
        
        if (r < 0.8) {
            // Short tasks
            exec_time = Uniform(1.0, 5.0);
            nice = -5;
        } else {
            // Long tasks
            exec_time = Uniform(50.0, 100.0);
            nice = 0;
        }
        
        tasks.push_back(std::make_shared<Task>(i, arrival_time, exec_time, nice));
        
        SelectStream(0);
    }
    
    return tasks;
}

} // namespace sched_sim