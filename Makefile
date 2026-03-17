CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I include
LDFLAGS =

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/scheduler_sim

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

# Run all schedulers on test workload
run: $(TARGET)
	./$(TARGET) workloads/test_workload.csv all

# Quick test with RR
test: $(TARGET)
	./$(TARGET) workloads/test_workload.csv rr
