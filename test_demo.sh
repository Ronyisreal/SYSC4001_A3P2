#!/bin/bash

# Test script for TA Marking System - Part 2a

echo "======================================"
echo "TA Marking System - Part 2a Test"
echo "======================================"
echo ""

# Check if program is compiled
if [ ! -f "./ta_marking" ]; then
    echo "Program not compiled. Compiling now..."
    make all
    echo ""
fi

# Check if test files exist
if [ ! -f "rubric.txt" ] || [ ! -f "exam_0001.txt" ]; then
    echo "Test files not found. Generating now..."
    make test_files
    echo ""
fi

echo "Initial Rubric Content:"
echo "----------------------"
cat rubric.txt
echo ""

echo "Test 1: Running with 2 TAs (10 second timeout)"
echo "-----------------------------------------------"
timeout 10 ./ta_marking 2
echo ""

echo "Modified Rubric After Test 1:"
echo "----------------------------"
cat rubric.txt
echo ""

# Reset test files for test 2
echo "Resetting test files for Test 2..."
make test_files > /dev/null 2>&1
echo ""

echo "Test 2: Running with 3 TAs (10 second timeout)"
echo "-----------------------------------------------"
timeout 10 ./ta_marking 3
echo ""

echo "Modified Rubric After Test 2:"
echo "----------------------------"
cat rubric.txt
echo ""

echo "======================================"
echo "Tests completed!"
echo "======================================"
echo ""
echo "Note: The program was stopped after 10 seconds for each test."
echo "In a full run, it would process all 20 exams until reaching student 9999."
echo ""
echo "Race Conditions Observed:"
echo "- Multiple TAs may modify the same rubric entry"
echo "- Multiple TAs may try to mark the same question"
echo "- These are expected in Part 2a and will be fixed in Part 2b with semaphores"
