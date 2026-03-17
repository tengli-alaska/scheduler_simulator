cmake_minimum_required(VERSION 3.15)

project(SchedulerSimulator
    VERSION 2.0.0
    DESCRIPTION "Server Queue-Based Scheduler Simulator with Trace-Driven Workloads"
    LANGUAGES CXX
)

# ============================================================================
# Options
# ============================================================================
option(ENABLE_SANITIZERS "Enable address sanitizers" OFF)
option(ENABLE_LTO "Enable Link Time Optimization" OFF)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Type
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# Sanitizers
if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address -fsanitize=undefined)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# LTO
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ============================================================================
# Output Directories
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Main Executable
# ============================================================================
add_executable(scheduler_sim
    src/main.cpp
)

target_include_directories(scheduler_sim PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link math library on Unix
if(UNIX)
    target_link_libraries(scheduler_sim PRIVATE m)
endif()

# ============================================================================
# Custom Targets
# ============================================================================
add_custom_target(run
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scheduler_sim
        ${PROJECT_SOURCE_DIR}/workloads/test_workload.csv all
    DEPENDS scheduler_sim
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Running all schedulers on test workload"
)

add_custom_target(quick-test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scheduler_sim
        ${PROJECT_SOURCE_DIR}/workloads/test_workload.csv rr 10
    DEPENDS scheduler_sim
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Running quick test (RR, 10 processes)"
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Scheduler Simulator Configuration")
message(STATUS "========================================")
message(STATUS "Version:      ${PROJECT_VERSION}")
message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "  LTO:        ${ENABLE_LTO}")
message(STATUS "========================================")
message(STATUS "")
