CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I include -I lib
CFLAGS = -std=c99 -O2 -Wall -I lib
LDFLAGS = -lm

BUILD_DIR = build

# C sources (random library)
C_SRCS = lib/rngs.c lib/rvgs.c lib/rvms.c
C_OBJS = $(C_SRCS:lib/%.c=$(BUILD_DIR)/%.o)

# C++ core sources
CXX_CORE_SRCS = src/core/task.cpp src/core/event.cpp src/core/simulator.cpp src/core/metrics.cpp
CXX_CORE_OBJS = $(CXX_CORE_SRCS:src/core/%.cpp=$(BUILD_DIR)/core_%.o)

# C++ workload sources
CXX_WL_SRCS = $(wildcard src/workloads/*.cpp)
CXX_WL_OBJS = $(CXX_WL_SRCS:src/workloads/%.cpp=$(BUILD_DIR)/wl_%.o)

# Main
MAIN_OBJ = $(BUILD_DIR)/main.o
TARGET = $(BUILD_DIR)/bin/scheduler_sim

.PHONY: all clean run quick-test

all: $(TARGET)

$(TARGET): $(C_OBJS) $(CXX_CORE_OBJS) $(CXX_WL_OBJS) $(MAIN_OBJ)
	@mkdir -p $(BUILD_DIR)/bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: lib/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/core_%.o: src/core/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/wl_%.o: src/workloads/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: apps/main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

quick-test: $(TARGET)
	./$(TARGET) -n 10
