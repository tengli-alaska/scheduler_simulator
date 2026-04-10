cmake_minimum_required(VERSION 3.15)

project(SchedulerSimulator
    VERSION 1.0.0
    DESCRIPTION "Multi-Scheduler Multi-Workload Evaluation Framework"
    LANGUAGES CXX C
)

# ============================================================================
# Options
# ============================================================================
option(BUILD_TESTS "Build unit tests" OFF)
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
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# External C Library
# ============================================================================
add_library(random_lib STATIC
    lib/rngs.c
    lib/rvgs.c
    lib/rvms.c
)

set_target_properties(random_lib PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(random_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/lib
)

# ============================================================================
# Core Library
# ============================================================================
add_library(scheduler_core STATIC
    src/core/task.cpp
    src/core/event.cpp
    src/core/simulator.cpp
    src/core/multi_core_simulator.cpp
    src/core/metrics.cpp
)

target_include_directories(scheduler_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_options(scheduler_core PRIVATE
    -include${PROJECT_SOURCE_DIR}/include/compat/builtin_compat.hpp
)

target_link_libraries(scheduler_core PUBLIC
    random_lib
)

# ============================================================================
# Workloads Library
# ============================================================================
add_library(workloads STATIC
    src/workloads/server_workload.cpp
    src/workloads/desktop_workload.cpp
    src/workloads/trace_replay_workload.cpp
)

target_include_directories(workloads PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_options(workloads PRIVATE
    -include${PROJECT_SOURCE_DIR}/include/compat/builtin_compat.hpp
)

target_link_libraries(workloads PUBLIC
    scheduler_core
    random_lib
)

# ============================================================================
# Main Executable
# ============================================================================
add_executable(scheduler_sim
    apps/main.cpp
)
target_compile_options(scheduler_sim PRIVATE
    -include${PROJECT_SOURCE_DIR}/include/compat/builtin_compat.hpp
)

target_link_libraries(scheduler_sim PRIVATE
    scheduler_core
    workloads
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
    DEPENDS scheduler_sim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running scheduler simulation"
)

add_custom_target(quick-test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scheduler_sim -n 10
    DEPENDS scheduler_sim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running quick test (10 tasks)"
)

add_custom_target(clean-results
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/bin/results.csv
    COMMENT "Removing result files"
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
message(STATUS "  Tests:      ${BUILD_TESTS}")
message(STATUS "  Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "  LTO:        ${ENABLE_LTO}")
message(STATUS "========================================")
message(STATUS "")
