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

class ServerWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "Server"; }
    std::string description() const override {
        return "Web/app server: bursty requests, API handling, background jobs, rare heavy compute";
    }
};

class DesktopWorkload : public WorkloadGenerator {
public:
    std::vector<TaskPtr> generate(uint32_t num_tasks) override;
    std::string name() const override { return "Desktop"; }
    std::string description() const override {
        return "Interactive desktop: rapid UI tasks, shell commands, compilations, background indexing";
    }
};

} // namespace sched_sim
