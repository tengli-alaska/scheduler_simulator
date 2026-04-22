/**
 * @file main.cpp
 * @brief Main experiment runner for scheduler evaluation
 */

#include "scheduler/simulator.hpp"
#include "scheduler/multi_core_simulator.hpp"
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
#include <ctime>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

extern "C" {
    #include "rngs.h"
}

using namespace sched_sim;

static const char* METRICS_SCHEMA_VERSION = "metrics.v2";
static const char* TASK_SCHEMA_VERSION = "tasks.v2";

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool is_valid_topology(const std::string& topology) {
    return topology == "sq" || topology == "mq";
}

bool is_valid_balancer(const std::string& balancer) {
    return balancer == "rr" || balancer == "leastloaded";
}

struct Config {
    uint32_t num_tasks = 100;
    int num_cores = 1;
    int num_replications = 1;
    double stop_time = 100000.0;
    std::string scheduler_filter = "all";
    std::string workload_filter = "all";
    std::string topology = "sq";          // "sq" = single-queue, "mq" = multi-queue
    std::string balancer = "leastloaded"; // "rr" = round-robin, "leastloaded"
    bool work_stealing = true;
    std::string benchmark_version = "community-v1";
    std::string suite_id = "ad-hoc";
    std::string run_id;
    long seed_base = 123456789L;
    std::string artifact_dir;
};

std::string make_default_run_id() {
    return "run-" + std::to_string(std::time(nullptr)) + "-" + std::to_string(getpid());
}

std::string sanitize_id(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') {
            out.push_back(ch);
        } else {
            out.push_back('-');
        }
    }
    if (out.empty()) return "run-unknown";
    return out;
}

std::string default_artifact_dir(const Config& config) {
    return "runs/" + sanitize_id(config.suite_id) + "/" + sanitize_id(config.run_id);
}

std::string work_stealing_label(const Config& config) {
    return config.work_stealing ? "on" : "off";
}

void write_metrics_header(std::ostream& out) {
    out << "BenchmarkVersion,SchemaVersion,SuiteId,RunId,Seed,"
        << "Replication,NumTasks,Cores,Topology,Balancer,WorkStealing,StopTime,"
        << "Scheduler,Workload,Completed,CompletionRatio,MeanRT,"
        << "P95RT,P99RT,MeanTAT,MeanWT,Throughput,ThroughputPerCore,"
        << "Utilization,JainsFairness,ContextSwitches,Preemptions\n";
}

void write_metrics_row(const Metrics& metrics,
                       const std::string& scheduler_name,
                       const std::string& workload_name,
                       const Config& config,
                       long seed,
                       int replication,
                       std::ostream& out) {
    const double completion_ratio =
        (config.num_tasks > 0) ? (static_cast<double>(metrics.num_completed) / config.num_tasks) : 0.0;
    const double throughput_per_core =
        metrics.throughput / static_cast<double>(std::max(1, config.num_cores));

    out << config.benchmark_version << ","
        << METRICS_SCHEMA_VERSION << ","
        << config.suite_id << ","
        << config.run_id << ","
        << seed << ","
        << replication << ","
        << config.num_tasks << ","
        << config.num_cores << ","
        << config.topology << ","
        << (config.topology == "mq" ? config.balancer : "na") << ","
        << (config.topology == "mq" ? work_stealing_label(config) : "na") << ","
        << config.stop_time << ","
        << scheduler_name << ","
        << workload_name << ","
        << metrics.num_completed << ","
        << completion_ratio << ","
        << metrics.mean_response_time << ","
        << metrics.p95_response_time << ","
        << metrics.p99_response_time << ","
        << metrics.mean_turnaround_time << ","
        << metrics.mean_wait_time << ","
        << metrics.throughput << ","
        << throughput_per_core << ","
        << metrics.utilization << ","
        << metrics.jains_fairness << ","
        << metrics.context_switches << ","
        << metrics.preemptions << "\n";
}

void write_task_header(std::ostream& out) {
    out << "BenchmarkVersion,SchemaVersion,SuiteId,RunId,Seed,"
        << "Replication,NumTasks,Cores,Topology,Balancer,WorkStealing,StopTime,"
        << "Scheduler,Workload,TaskID,Nice,Weight,Arrival,Execution,"
        << "AllocatedCPU,Remaining,Started,Completed,StartTime,CompletionTime,"
        << "ResponseTime,TurnaroundTime,WaitTime,PreemptionCount\n";
}

void write_task_rows(const std::vector<TaskPtr>& tasks,
                     const std::string& scheduler_name,
                     const std::string& workload_name,
                     const Config& config,
                     long seed,
                     int replication,
                     std::ostream& out) {
    for (const auto& task : tasks) {
        const bool started = task->start_time() >= 0.0;
        const bool completed = task->completion_time() >= 0.0;
        const double allocated_cpu = task->execution_time() - task->remaining_time();

        out << config.benchmark_version << ","
            << TASK_SCHEMA_VERSION << ","
            << config.suite_id << ","
            << config.run_id << ","
            << seed << ","
            << replication << ","
            << config.num_tasks << ","
            << config.num_cores << ","
            << config.topology << ","
            << (config.topology == "mq" ? config.balancer : "na") << ","
            << (config.topology == "mq" ? work_stealing_label(config) : "na") << ","
            << config.stop_time << ","
            << scheduler_name << ","
            << workload_name << ","
            << task->id() << ","
            << task->nice() << ","
            << task->weight() << ","
            << task->arrival_time() << ","
            << task->execution_time() << ","
            << allocated_cpu << ","
            << task->remaining_time() << ","
            << (started ? 1 : 0) << ","
            << (completed ? 1 : 0) << ","
            << task->start_time() << ","
            << task->completion_time() << ","
            << task->response_time() << ","
            << task->turnaround_time() << ","
            << task->wait_time() << ","
            << task->preemption_count() << "\n";
    }
}

void print_banner() {
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "  MULTI-SCHEDULER MULTI-WORKLOAD EVALUATION\n";
    std::cout << "  \n";
    std::cout << "  Schedulers:  CFS, EEVDF, MLFQ, Stride\n";
    std::cout << "  Workloads:   Server, Desktop, Google, Alibaba\n";
    std::cout << "  Topologies:  Single-Queue, Multi-Queue\n";
    std::cout << "====================================================\n";
    std::cout << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -n <num>       Number of tasks per workload (default: 100)\n";
    std::cout << "  -c <num>       Number of CPU cores (default: 1)\n";
    std::cout << "  -r <num>       Number of replications (default: 1)\n";
    std::cout << "  -t <time>      Simulation stop time (default: 100000.0)\n";
    std::cout << "  -s <scheduler> Scheduler to use (default: all)\n";
    std::cout << "                 Options: cfs, eevdf, mlfq, stride, all\n";
    std::cout << "  -w <workload>  Workload to use (default: all)\n";
    std::cout << "                 Options: server, desktop, google, alibaba, all\n";
    std::cout << "  -m <topology>  Queue topology (default: sq)\n";
    std::cout << "                 sq = single-queue multi-server (shared queue)\n";
    std::cout << "                 mq = multi-queue multi-server (per-core queues)\n";
    std::cout << "  -b <balancer>  Load balancer for mq topology (default: leastloaded)\n";
    std::cout << "                 rr = round-robin, leastloaded = least-loaded\n";
    std::cout << "  --benchmark-version <id>  Benchmark version tag (default: community-v1)\n";
    std::cout << "  --suite-id <id>           Benchmark suite id (default: ad-hoc)\n";
    std::cout << "  --run-id <id>             Explicit run id (default: auto timestamp)\n";
    std::cout << "  --seed <num>              Base RNG seed (default: 123456789)\n";
    std::cout << "  --artifact-dir <path>     Output directory for metrics/tasks artifacts\n";
    std::cout << "  --no-steal     Disable work stealing in mq topology\n";
    std::cout << "  -h             Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -n 200 -c 2\n";
    std::cout << "  " << program_name << " -s cfs -w server\n";
    std::cout << "  " << program_name << " -s eevdf -w desktop -n 50\n";
    std::cout << "  " << program_name << " -m mq -c 4 -b leastloaded\n";
    std::cout << "  " << program_name << " -m mq -c 4 -b rr --no-steal\n";
    std::cout << "  " << program_name << " -s stride -c 4 -n 500\n";
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
        } else if (std::strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.topology = to_lower(argv[++i]);
        } else if (std::strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            config.balancer = to_lower(argv[++i]);
        } else if (std::strcmp(argv[i], "--benchmark-version") == 0 && i + 1 < argc) {
            config.benchmark_version = argv[++i];
        } else if (std::strcmp(argv[i], "--suite-id") == 0 && i + 1 < argc) {
            config.suite_id = argv[++i];
        } else if (std::strcmp(argv[i], "--run-id") == 0 && i + 1 < argc) {
            config.run_id = argv[++i];
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            config.seed_base = std::atol(argv[++i]);
        } else if (std::strcmp(argv[i], "--artifact-dir") == 0 && i + 1 < argc) {
            config.artifact_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--no-steal") == 0) {
            config.work_stealing = false;
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

void validate_config_or_exit(const Config& config) {
    if (config.num_tasks == 0) {
        std::cerr << "Error: Number of tasks must be >= 1\n";
        std::exit(1);
    }
    if (config.num_cores < 1) {
        std::cerr << "Error: Number of CPU cores must be >= 1\n";
        std::exit(1);
    }
    if (config.num_replications < 1) {
        std::cerr << "Error: Number of replications must be >= 1\n";
        std::exit(1);
    }
    if (config.stop_time <= 0.0) {
        std::cerr << "Error: Stop time must be > 0\n";
        std::exit(1);
    }
    if (!is_valid_topology(config.topology)) {
        std::cerr << "Error: Unknown topology '" << config.topology << "'\n";
        std::cerr << "Valid options: sq, mq\n";
        std::exit(1);
    }
    if (!is_valid_balancer(config.balancer)) {
        std::cerr << "Error: Unknown load balancer '" << config.balancer << "'\n";
        std::cerr << "Valid options: rr, leastloaded\n";
        std::exit(1);
    }
    if (config.seed_base <= 0) {
        std::cerr << "Error: Seed must be > 0\n";
        std::exit(1);
    }
    if (config.benchmark_version.empty()) {
        std::cerr << "Error: benchmark-version must be non-empty\n";
        std::exit(1);
    }
    if (config.suite_id.empty()) {
        std::cerr << "Error: suite-id must be non-empty\n";
        std::exit(1);
    }
    if (!config.artifact_dir.empty() && config.artifact_dir.find('\0') != std::string::npos) {
        std::cerr << "Error: artifact-dir contains invalid characters\n";
        std::exit(1);
    }
}

void run_experiment(
    std::function<SchedulerPtr()> scheduler_factory,
    const std::string& scheduler_name,
    WorkloadGenerator& workload_gen,
    const Config& config,
    long seed,
    int replication,
    std::ostream& csv_out,
    std::ostream& task_out)
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
    metrics.calculate(sim.completed_tasks(), sim.current_time(), config.num_cores);
    metrics.context_switches = sim.context_switches();
    metrics.preemptions = sim.preemptions();

    // Print results
    metrics.print(scheduler_name, workload_gen.name());

    // Write aggregate + task-level outputs
    write_metrics_row(metrics, scheduler_name, workload_gen.name(),
                      config, seed, replication, csv_out);
    write_task_rows(tasks, scheduler_name, workload_gen.name(),
                    config, seed, replication, task_out);
}

void run_experiment_mq(
    std::function<SchedulerPtr()> scheduler_factory,
    const std::string& scheduler_name,
    WorkloadGenerator& workload_gen,
    const Config& config,
    long seed,
    int replication,
    std::ostream& csv_out,
    std::ostream& task_out)
{
    std::cout << "\n[" << scheduler_name << " on " << workload_gen.name()
              << " (multi-queue, " << config.balancer << ")]\n";

    // Generate workload
    auto tasks = workload_gen.generate(config.num_tasks);

    // Create load balancer
    LoadBalancerPtr balancer;
    if (config.balancer == "rr") {
        balancer = std::make_unique<RoundRobinBalancer>();
    } else {
        balancer = std::make_unique<LeastLoadedBalancer>();
    }

    // Create multi-core simulator
    MultiCoreSimulator sim(scheduler_factory, config.num_cores,
                           std::move(balancer), config.work_stealing);

    // Add tasks
    for (auto& task : tasks) {
        sim.add_task(task);
    }

    // Run simulation
    sim.run(config.stop_time);

    // Calculate metrics
    Metrics metrics;
    metrics.calculate(sim.completed_tasks(), sim.current_time(), config.num_cores);
    metrics.context_switches = sim.context_switches();
    metrics.preemptions = sim.preemptions();

    // Print results
    metrics.print(scheduler_name, workload_gen.name());

    // Write aggregate + task-level outputs
    write_metrics_row(metrics, scheduler_name, workload_gen.name(),
                      config, seed, replication, csv_out);
    write_task_rows(tasks, scheduler_name, workload_gen.name(),
                    config, seed, replication, task_out);
}

int main(int argc, char* argv[]) {
    print_banner();
    
    Config config = parse_args(argc, argv);
    validate_config_or_exit(config);
    
    std::cout << "Configuration:\n";
    std::cout << "  Tasks per workload: " << config.num_tasks << "\n";
    std::cout << "  CPU cores:          " << config.num_cores << "\n";
    std::cout << "  Replications:       " << config.num_replications << "\n";
    std::cout << "  Stop time:          " << config.stop_time << "\n";
    std::cout << "  Scheduler:          " << config.scheduler_filter << "\n";
    std::cout << "  Workload:           " << config.workload_filter << "\n";
    std::cout << "  Topology:           " << (config.topology == "mq" ? "multi-queue" : "single-queue") << "\n";
    std::cout << "  Benchmark version:  " << config.benchmark_version << "\n";
    std::cout << "  Suite id:           " << config.suite_id << "\n";
    std::cout << "  Seed base:          " << config.seed_base << "\n";
    std::cout << "  Artifact dir:       "
              << (config.artifact_dir.empty() ? "(auto)" : config.artifact_dir) << "\n";
    if (config.topology == "mq") {
        std::cout << "  Load balancer:      " << config.balancer << "\n";
        std::cout << "  Work stealing:      " << (config.work_stealing ? "on" : "off") << "\n";
    }
    std::cout << "\n";
    
    // Initialize RNG
    PlantSeeds(config.seed_base);

    if (config.run_id.empty()) {
        config.run_id = make_default_run_id();
    }
    config.run_id = sanitize_id(config.run_id);

    // Create output artifact directory: runs/<suite_id>/<run_id>/ by default.
    std::string artifact_dir = config.artifact_dir.empty()
        ? default_artifact_dir(config)
        : config.artifact_dir;
    std::filesystem::create_directories(artifact_dir);

    // Canonical artifact filenames for isolated run directories.
    std::string csv_filename = artifact_dir + "/metrics.csv";

    // Open aggregate CSV output
    std::ofstream csv_file(csv_filename);
    if (!csv_file) {
        std::cerr << "Error: Could not open " << csv_filename << " for writing\n";
        return 1;
    }
    write_metrics_header(csv_file);

    // Open task-level CSV output
    std::string task_csv_filename = artifact_dir + "/tasks.csv";
    std::ofstream task_csv_file(task_csv_filename);
    if (!task_csv_file) {
        std::cerr << "Error: Could not open " << task_csv_filename << " for writing\n";
        return 1;
    }
    write_task_header(task_csv_file);
    
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
    all_workloads.push_back({"google", std::make_unique<TraceReplayWorkload>(TraceType::GoogleV3)});
    all_workloads.push_back({"alibaba", std::make_unique<TraceReplayWorkload>(TraceType::AlibabaV2018)});

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
            std::cerr << "Valid options: server, desktop, google, alibaba, all\n";
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
                long seed = config.seed_base + (rep * 1000);
                PlantSeeds(seed);
                
                if (config.topology == "mq") {
                    run_experiment_mq(sched_factory, sched_name, *workload_gen,
                                     config, seed, rep + 1, csv_file, task_csv_file);
                } else {
                    run_experiment(sched_factory, sched_name, *workload_gen,
                                 config, seed, rep + 1, csv_file, task_csv_file);
                }
            }
        }
    }
    
    csv_file.close();
    task_csv_file.close();
    
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "  ALL EXPERIMENTS COMPLETED\n";
    std::cout << "====================================================\n";
    std::cout << "\n";
    std::cout << "Results saved to: " << csv_filename << "\n";
    std::cout << "Task data saved to: " << task_csv_filename << "\n";
    std::cout << "\n";
    
    return 0;
}
