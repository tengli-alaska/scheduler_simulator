#include "scheduler/workload.hpp"

extern "C" {
    #include "rngs.h"
    #include "rvgs.h"
}

namespace sched_sim {

std::vector<TaskPtr> CPUBoundWorkload::generate(uint32_t num_tasks) {
    std::vector<TaskPtr> tasks;
    tasks.reserve(num_tasks);
    
    double arrival_time = 0.0;
    
    SelectStream(0);
    
    for (uint32_t i = 0; i < num_tasks; ++i) {
        arrival_time += Exponential(2.0);
        
        SelectStream(1);
        double exec_time = Uniform(20.0, 50.0);
        
        SelectStream(2);
        double r = Random();
        int nice;
        
        if (r < 0.7) {
            nice = 0;
        } else if (r < 0.9) {
            nice = -5;
        } else {
            nice = 5;
        }
        
        tasks.push_back(std::make_shared<Task>(i, arrival_time, exec_time, nice));
        
        SelectStream(0);
    }
    
    return tasks;
}

} // namespace sched_sim