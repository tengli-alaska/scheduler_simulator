#include "simulator.h"
#include "workload_loader.h"
#include "rr_scheduler.h"
#include "cfs_scheduler.h"
#include "eevdf_scheduler.h"
#include "stride_scheduler.h"
#include "mlfq_scheduler.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>
#include <vector>

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <workload.csv> [scheduler] [max_processes]" << std::endl;
    std::cerr << "  scheduler: rr, cfs, eevdf, stride, mlfq, all (default: rr)" << std::endl;
    std::cerr << "  max_processes: limit number of processes loaded (0 = all)" << std::endl;
}

std::unique_ptr<Scheduler> create_scheduler(const std::string& name) {
    if (name == "rr") {
        return std::make_unique<RRScheduler>(10000);
    } else if (name == "cfs") {
        return std::make_unique<CFSScheduler>();
    } else if (name == "eevdf") {
        return std::make_unique<EEVDFScheduler>();
    } else if (name == "stride") {
        return std::make_unique<StrideScheduler>();
    } else if (name == "mlfq") {
        return std::make_unique<MLFQScheduler>();
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string workload_path = argv[1];
    std::string scheduler_name = (argc > 2) ? argv[2] : "rr";
    int max_procs = (argc > 3) ? std::atoi(argv[3]) : 0;

    // Load workload
    auto processes = load_workload_csv(workload_path, max_procs);
    if (processes.empty()) {
        std::cerr << "No processes loaded." << std::endl;
        return 1;
    }

    // Determine which schedulers to run
    std::vector<std::string> to_run;
    if (scheduler_name == "all") {
        to_run = {"rr", "cfs", "eevdf", "stride", "mlfq"};
    } else {
        to_run = {scheduler_name};
    }

    for (const auto& sname : to_run) {
        auto scheduler = create_scheduler(sname);
        if (!scheduler) {
            std::cerr << "Unknown scheduler: " << sname << std::endl;
            std::cerr << "Available: rr, cfs, eevdf, stride, mlfq, all" << std::endl;
            return 1;
        }

        // Each run gets a fresh copy of the processes
        Simulator sim(scheduler.get(), processes);
        sim.run(true);
    }

    return 0;
}
