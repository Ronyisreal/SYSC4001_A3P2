# Makefile for SYSC4001 Assignment 3 Part 2
# Concurrent TA Marking System

CXX = g++
CXXFLAGS = -Wall -g -std=c++11
STUDENT_SUFFIX = 101116888_101276841

# Target executables
TARGET_2A = ta_marking_$(STUDENT_SUFFIX)
TARGET_2B = ta_marking_semaphore_$(STUDENT_SUFFIX)

# Source files
SOURCE_2A = ta_marking_$(STUDENT_SUFFIX).cpp
SOURCE_2B = ta_marking_semaphore_$(STUDENT_SUFFIX).cpp

# Default target - build everything
all: part2a part2b

# Part 2a - No synchronization
part2a: $(SOURCE_2A)
	@echo "Compiling Part 2a (no synchronization)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET_2A) $(SOURCE_2A)
	@echo "Part 2a compiled successfully!"

# Part 2b - With semaphores
part2b: $(SOURCE_2B)
	@echo "Compiling Part 2b (with semaphores)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET_2B) $(SOURCE_2B)
	@echo "Part 2b compiled successfully!"

# Generate test files
test_files:
	@echo "Generating test files..."
	chmod +x generate_test_files.sh
	./generate_test_files.sh

# Run Part 2a with 2 TAs
run2a: part2a test_files
	@echo "Running Part 2a with 2 TAs..."
	./$(TARGET_2A) 2

# Run Part 2a with 3 TAs
run3a: part2a test_files
	@echo "Running Part 2a with 3 TAs..."
	./$(TARGET_2A) 3

# Run Part 2b with 2 TAs
run2b: part2b test_files
	@echo "Running Part 2b with 2 TAs..."
	./$(TARGET_2B) 2

# Run Part 2b with 3 TAs
run3b: part2b test_files
	@echo "Running Part 2b with 3 TAs..."
	./$(TARGET_2B) 3

# Run Part 2b with 4 TAs
run4b: part2b test_files
	@echo "Running Part 2b with 4 TAs..."
	./$(TARGET_2B) 4

# Quick test - run both versions with 2 TAs
test: part2a part2b test_files
	@echo "====== Testing Part 2a ======"
	timeout 10 ./$(TARGET_2A) 2 || true
	@echo ""
	@echo "====== Resetting test files ======"
	./generate_test_files.sh > /dev/null 2>&1
	@echo ""
	@echo "====== Testing Part 2b ======"
	timeout 10 ./$(TARGET_2B) 2 || true

# Compare Part 2a and 2b outputs
compare: part2a part2b test_files
	@echo "Running Part 2a..."
	timeout 8 ./$(TARGET_2A) 2 > output_2a.txt 2>&1 || true
	@echo "Resetting test files..."
	./generate_test_files.sh > /dev/null 2>&1
	@echo "Running Part 2b..."
	timeout 8 ./$(TARGET_2B) 2 > output_2b.txt 2>&1 || true
	@echo "Outputs saved to output_2a.txt and output_2b.txt"
	@echo "Use 'diff output_2a.txt output_2b.txt' to compare"

# Clean compiled files
clean:
	@echo "Cleaning compiled files..."
	rm -f $(TARGET_2A) $(TARGET_2B)
	rm -f output_*.txt

# Clean everything including test files
cleanall: clean
	@echo "Cleaning test files..."
	rm -f exam_*.txt
	rm -f rubric.txt
	@echo "Cleaning shared memory and semaphores..."
	@ipcs -m | grep $(USER) | awk '{print $$2}' | xargs -r ipcrm -m 2>/dev/null || true
	@ipcs -s | grep $(USER) | awk '{print $$2}' | xargs -r ipcrm -s 2>/dev/null || true

# Show help
help:
	@echo "SYSC4001 Assignment 3 Part 2 - Makefile"
	@echo "========================================"
	@echo ""
	@echo "Build targets:"
	@echo "  make all          - Compile both Part 2a and 2b"
	@echo "  make part2a       - Compile Part 2a only"
	@echo "  make part2b       - Compile Part 2b only"
	@echo ""
	@echo "Test file generation:"
	@echo "  make test_files   - Generate rubric and exam files"
	@echo ""
	@echo "Run targets:"
	@echo "  make run2a        - Run Part 2a with 2 TAs"
	@echo "  make run3a        - Run Part 2a with 3 TAs"
	@echo "  make run2b        - Run Part 2b with 2 TAs"
	@echo "  make run3b        - Run Part 2b with 3 TAs"
	@echo "  make run4b        - Run Part 2b with 4 TAs"
	@echo ""
	@echo "Testing:"
	@echo "  make test         - Run both versions briefly"
	@echo "  make compare      - Compare Part 2a vs 2b outputs"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean        - Remove compiled files"
	@echo "  make cleanall     - Remove everything including test files"
	@echo ""
	@echo "Help:"
	@echo "  make help         - Show this help message"

# Check for shared memory/semaphore leaks
check:
	@echo "Checking for shared memory segments..."
	@ipcs -m | grep $(USER) || echo "No shared memory segments found"
	@echo ""
	@echo "Checking for semaphore sets..."
	@ipcs -s | grep $(USER) || echo "No semaphore sets found"

.PHONY: all part2a part2b test_files run2a run3a run2b run3b run4b test compare clean cleanall help check
