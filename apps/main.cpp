/**
 * @file main.cpp
 * @brief Main experiment runner for scheduler evaluation
 */

#include "scheduler/simulator.hpp"
#include "scheduler/metrics.hpp"
#include "scheduler/workload.hpp"
#include "schedulers/cfs_scheduler.hpp"
#include "schedulers/eevdf_scheduler.hpp"
#include "schedulers/mlfq_scheduler.hpp"
#include "schedulers/stride_scheduler.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <cstring>
#include <algorithm>

extern "C" {
    #include "rngs.h"
}

using namespace sched_sim;

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

struct Config {
    uint32_t num_tasks = 100;
    int num_cores = 1;
    int num_replications = 1;
    double stop_time = 10000.0;
    std::string scheduler_filter = "all";
    std::string workload_filter = "all";
};

void print_banner() {
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "  MULTI-SCHEDULER MULTI-WORKLOAD EVALUATION\n";
    std::cout << "  \n";
    std::cout << "  Schedulers: CFS, EEVDF, MLFQ, Stride\n";
    std::cout << "  Workloads:  Server, Desktop\n";
    std::cout << "====================================================\n";
    std::cout << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -n <num>       Number of tasks per workload (default: 100)\n";
    std::cout << "  -c <num>       Number of CPU cores (default: 1)\n";
    std::cout << "  -r <num>       Number of replications (default: 1)\n";
    std::cout << "  -t <time>      Simulation stop time (default: 10000.0)\n";
    std::cout << "  -s <scheduler> Scheduler to use (default: all)\n";
    std::cout << "                 Options: cfs, eevdf, mlfq, stride, all\n";
    std::cout << "  -w <workload>  Workload to use (default: all)\n";
    std::cout << "                 Options: server, desktop, all\n";
    std::cout << "  -h             Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -n 200 -c 2\n";
    std::cout << "  " << program_name << " -s cfs -w server\n";
    std::cout << "  " << program_name << " -s eevdf -w desktop -n 50\n";
    std::cout << "\n";
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            config.num_tasks = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config.num_cores = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            config.num_replications = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.stop_time = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            config.scheduler_filter = to_lower(argv[++i]);
        } else if (std::strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.workload_filter = to_lower(argv[++i]);
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n\n";
            print_usage(argv[0]);
            std::exit(1);
        }
    }
    
    return config;
}

void run_experiment(
    std::function<SchedulerPtr()> scheduler_factory,
    const std::string& scheduler_name,
    WorkloadGenerator& workload_gen,
    const Config& config,
    std::ostream& csv_out)
{
    std::cout << "\n[" << scheduler_name << " on " << workload_gen.name() << "]\n";
    
    // Generate workload
    auto tasks = workload_gen.generate(config.num_tasks);
    
    // Create scheduler and simulator
    auto scheduler = scheduler_factory();
    Simulator sim(std::move(scheduler), config.num_cores);
    
    // Add tasks
    for (auto& task : tasks) {
        sim.add_task(task);
    }
    
    // Run simulation
    sim.run(config.stop_time);
    
    // Calculate metrics
    Metrics metrics;
    metrics.calculate(sim.completed_tasks(), sim.current_time());
    metrics.context_switches = sim.scheduler()->context_switches();
    metrics.preemptions = sim.scheduler()->preemptions();
    
    // Print results
    metrics.print(scheduler_name, workload_gen.name());
    
    // Write to CSV
    metrics.to_csv(scheduler_name, workload_gen.name(), csv_out);
}

int main(int argc, char* argv[]) {
    print_banner();
    
    Config config = parse_args(argc, argv);
    
    std::cout << "Configuration:\n";
    std::cout << "  Tasks per workload: " << config.num_tasks << "\n";
    std::cout << "  CPU cores:          " << config.num_cores << "\n";
    std::cout << "  Replications:       " << config.num_replications << "\n";
    std::cout << "  Stop time:          " << config.stop_time << "\n";
    std::cout << "  Scheduler:          " << config.scheduler_filter << "\n";
    std::cout << "  Workload:           " << config.workload_filter << "\n";
    std::cout << "\n";
    
    // Initialize RNG
    PlantSeeds(123456789L);
    
    // Open CSV output
    std::ofstream csv_file("results.csv");
    if (!csv_file) {
        std::cerr << "Error: Could not open results.csv for writing\n";
        return 1;
    }
    
    csv_file << "Scheduler,Workload,Completed,MeanRT,"
             << "P95RT,P99RT,MeanTAT,MeanWT,Throughput,JainsFairness,"
             << "ContextSwitches,Preemptions\n";
    
    // Define all schedulers
    std::vector<std::pair<std::string, std::function<SchedulerPtr()>>> all_schedulers = {
        {"CFS", []() { return std::make_unique<CFSScheduler>(); }},
        {"EEVDF", []() { return std::make_unique<EEVDFScheduler>(); }},
        {"MLFQ", []() { return std::make_unique<MLFQScheduler>(); }},
        {"Stride", []() { return std::make_unique<StrideScheduler>(); }}
    };

    // Filter schedulers
    std::vector<std::pair<std::string, std::function<SchedulerPtr()>>> schedulers;
    if (config.scheduler_filter == "all") {
        schedulers = all_schedulers;
    } else {
        for (const auto& s : all_schedulers) {
            if (to_lower(s.first) == config.scheduler_filter) {
                schedulers.push_back(s);
                break;
            }
        }
        if (schedulers.empty()) {
            std::cerr << "Error: Unknown scheduler '" << config.scheduler_filter << "'\n";
            std::cerr << "Valid options: cfs, eevdf, mlfq, stride, all\n";
            return 1;
        }
    }

    // Define all workloads with keys for filtering
    struct WorkloadEntry {
        std::string key;
        std::unique_ptr<WorkloadGenerator> generator;
    };
    std::vector<WorkloadEntry> all_workloads;
    all_workloads.push_back({"server", std::make_unique<ServerWorkload>()});
    all_workloads.push_back({"desktop", std::make_unique<DesktopWorkload>()});

    // Filter workloads
    std::vector<std::unique_ptr<WorkloadGenerator>> workloads;
    if (config.workload_filter == "all") {
        for (auto& entry : all_workloads) {
            workloads.push_back(std::move(entry.generator));
        }
    } else {
        for (auto& entry : all_workloads) {
            if (entry.key == config.workload_filter) {
                workloads.push_back(std::move(entry.generator));
                break;
            }
        }
        if (workloads.empty()) {
            std::cerr << "Error: Unknown workload '" << config.workload_filter << "'\n";
            std::cerr << "Valid options: server, desktop, all\n";
            return 1;
        }
    }
    
    // Calculate total experiments
    int total_experiments = schedulers.size() * workloads.size() * config.num_replications;
    int current = 0;
    
    std::cout << "Total experiments: " << total_experiments << "\n\n";
    
    // Run all experiments
    for (int rep = 0; rep < config.num_replications; ++rep) {
        if (config.num_replications > 1) {
            std::cout << "\n";
            std::cout << "====================================================\n";
            std::cout << "REPLICATION " << (rep + 1) << " of " << config.num_replications << "\n";
            std::cout << "====================================================\n";
        }
        
        for (const auto& workload_gen : workloads) {
            std::cout << "\n";
            std::cout << "----------------------------------------------------\n";
            std::cout << "WORKLOAD: " << workload_gen->name() << "\n";
            std::cout << "Description: " << workload_gen->description() << "\n";
            std::cout << "----------------------------------------------------\n";
            
            for (const auto& [sched_name, sched_factory] : schedulers) {
                current++;
                std::cout << "\n[" << current << "/" << total_experiments << "] ";
                
                // Set seed for reproducibility
                long seed = 123456789L + (rep * 1000);
                PlantSeeds(seed);
                
                run_experiment(sched_factory, sched_name, *workload_gen, 
                             config, csv_file);
            }
        }
    }
    
    csv_file.close();
    
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "  ALL EXPERIMENTS COMPLETED\n";
    std::cout << "====================================================\n";
    std::cout << "\n";
    std::cout << "Results saved to: results.csv\n";
    std::cout << "\n";
    
    return 0;
}