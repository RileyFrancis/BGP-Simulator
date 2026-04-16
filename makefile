CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude

# Directories
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build

# Source files (matching your actual filenames)
SOURCES = $(SRC_DIR)/as.cpp $(SRC_DIR)/as_graph.cpp $(SRC_DIR)/utils.cpp
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Create build directory if it doesn't exist
$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(TEST_DIR))

# Test executable
test_graph: $(TEST_DIR)/test_graph.cpp $(BUILD_DIR)/as.o $(BUILD_DIR)/as_graph.o
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_graph $(TEST_DIR)/test_graph.cpp $(BUILD_DIR)/as.o $(BUILD_DIR)/as_graph.o

# Main program
bgp_simulator: $(SRC_DIR)/main.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o bgp_simulator $(SRC_DIR)/main.cpp $(OBJECTS)

# Pattern rule for object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BUILD_DIR)/*.o test_graph bgp_simulator

.PHONY: clean