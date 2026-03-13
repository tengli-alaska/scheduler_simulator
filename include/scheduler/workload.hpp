/**
 * @file workload.hpp
 * @brief Workload generator interface and implementations
 */

#pragma once

#include "task.hpp"
#include <vector>
#include <string>
#include <memory>

namespace sched_sim {

/**
 * @class WorkloadGenerator
 * @brief Abstract base class for workload generators
 */
class WorkloadGenerator {
public:
    virtual ~WorkloadGenerator() = default;
    
    /**
     * @brief Generate a workload
     * @param num_tasks Number of tasks to generate
     * @return Vector of generated tasks
     */
    virtual std::vector<TaskPtr> generate(uint32_t num_tasks) = 0;
    
    /**
     * @brief Get workload name
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief Get workload description
     */
    virtual std::string description() const = 0;
};

using WorkloadPtr = std::unique_ptr<WorkloadGenerator>;

// Concrete workload generators
class CPUBoundWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "CPU-Bound"; }
    std::string description() const override {
        return "Compute-intensive tasks with no I/O";
    }
};

class IOBoundWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "I/O-Bound"; }
    std::string description() const override {
        return "Interactive tasks with short CPU bursts";
    }
};

class MixedWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "Mixed"; }
    std::string description() const override {
        return "Realistic mix: batch, interactive, background";
    }
};

class BurstyWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "Bursty"; }
    std::string description() const override {
        return "Alternating bursts and quiet periods";
    }
};

class BimodalWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "Bimodal"; }
    std::string description() const override {
        return "80% short tasks, 20% very long tasks";
    }
};

} // namespace sched_sim