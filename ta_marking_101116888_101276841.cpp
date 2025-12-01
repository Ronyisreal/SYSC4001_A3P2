/**
 * @file ta_marking.cpp
 * @brief Assignment 3 Part 2a - Concurrent TA Marking System
 * @author Student Implementation
 * 
 * This program simulates multiple Teaching Assistants (TAs) marking exams concurrently.
 * Each TA can read/modify the rubric and mark individual questions on exams.
 * 
 * Compile: g++ -o ta_marking ta_marking.cpp
 * Run: ./ta_marking <number_of_TAs>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <iomanip>

// Constants
#define NUM_EXERCISES 5
#define MAX_EXAMS 100
#define RUBRIC_FILE "rubric.txt"
#define EXAM_PREFIX "exam_"
#define EXAM_SUFFIX ".txt"

// Shared memory structure for rubric
struct Rubric {
    char exercises[NUM_EXERCISES][100];  // Each exercise rubric
};

// Shared memory structure for current exam
struct CurrentExam {
    int student_number;
    bool questions_marked[NUM_EXERCISES];  // Track which questions are marked
    int exam_index;  // Current exam being processed
};

// Function to generate random delay between min and max seconds
void random_delay(double min_sec, double max_sec) {
    double random_time = min_sec + (max_sec - min_sec) * ((double)rand() / RAND_MAX);
    int microseconds = (int)(random_time * 1000000);
    usleep(microseconds);
}

// Function to load rubric from file into shared memory
void load_rubric(Rubric* rubric) {
    std::ifstream file(RUBRIC_FILE);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open rubric file" << std::endl;
        return;
    }
    
    std::string line;
    int index = 0;
    while (std::getline(file, line) && index < NUM_EXERCISES) {
        strncpy(rubric->exercises[index], line.c_str(), 99);
        rubric->exercises[index][99] = '\0';
        index++;
    }
    file.close();
}

// Function to save rubric from shared memory to file
void save_rubric(Rubric* rubric) {
    std::ofstream file(RUBRIC_FILE);
    if (!file.is_open()) {
        std::cerr << "Error: Could not save rubric file" << std::endl;
        return;
    }
    
    for (int i = 0; i < NUM_EXERCISES; i++) {
        file << rubric->exercises[i] << std::endl;
    }
    file.close();
}

// Function to load exam into shared memory
bool load_exam(CurrentExam* exam, int exam_index) {
    // Generate filename
    std::stringstream ss;
    ss << EXAM_PREFIX << std::setfill('0') << std::setw(4) << exam_index << EXAM_SUFFIX;
    std::string filename = ss.str();
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    if (std::getline(file, line)) {
        exam->student_number = std::stoi(line);
        exam->exam_index = exam_index;
        // Reset all questions to unmarked
        for (int i = 0; i < NUM_EXERCISES; i++) {
            exam->questions_marked[i] = false;
        }
    }
    file.close();
    return true;
}

// Function to check if all questions are marked
bool all_questions_marked(CurrentExam* exam) {
    for (int i = 0; i < NUM_EXERCISES; i++) {
        if (!exam->questions_marked[i]) {
            return false;
        }
    }
    return true;
}

// TA process function
void ta_process(int ta_id, Rubric* rubric, CurrentExam* exam) {
    srand(time(NULL) + ta_id);  // Seed random number generator
    
    std::cout << "[TA " << ta_id << "] Started working" << std::endl;
    
    while (true) {
        // Check if we've reached the end (student 9999)
        if (exam->student_number == 9999) {
            std::cout << "[TA " << ta_id << "] Finished - reached student 9999" << std::endl;
            break;
        }
        
        std::cout << "[TA " << ta_id << "] Reviewing rubric for exam " 
                  << exam->student_number << std::endl;
        
        // Review rubric (iterate through each exercise)
        for (int i = 0; i < NUM_EXERCISES; i++) {
            // Random delay for reviewing (0.5-1.0 seconds)
            random_delay(0.5, 1.0);
            
            // Randomly decide if rubric needs correction (30% chance)
            if ((rand() % 100) < 30) {
                std::cout << "[TA " << ta_id << "] Correcting rubric for exercise " 
                          << (i + 1) << std::endl;
                
                // Find the character after the comma
                char* rubric_line = rubric->exercises[i];
                char* comma = strchr(rubric_line, ',');
                if (comma != NULL && *(comma + 1) == ' ') {
                    char& rubric_char = *(comma + 2);
                    std::cout << "[TA " << ta_id << "] Changing exercise " << (i + 1) 
                              << " rubric from '" << rubric_char << "' to '" 
                              << (char)(rubric_char + 1) << "'" << std::endl;
                    rubric_char++;
                    
                    // Save the modified rubric to file
                    save_rubric(rubric);
                }
            }
        }
        
        // Mark questions
        bool marked_something = false;
        for (int q = 0; q < NUM_EXERCISES; q++) {
            // Check if question is not yet marked
            if (!exam->questions_marked[q]) {
                // Mark this question
                exam->questions_marked[q] = true;
                marked_something = true;
                
                std::cout << "[TA " << ta_id << "] Marking student " 
                          << exam->student_number << ", question " << (q + 1) << std::endl;
                
                // Random delay for marking (1.0-2.0 seconds)
                random_delay(1.0, 2.0);
                
                std::cout << "[TA " << ta_id << "] Finished marking student " 
                          << exam->student_number << ", question " << (q + 1) << std::endl;
                
                break;  // Mark only one question at a time
            }
        }
        
        // If all questions are marked, load next exam
        if (all_questions_marked(exam)) {
            std::cout << "[TA " << ta_id << "] All questions marked for student " 
                      << exam->student_number << ". Loading next exam..." << std::endl;
            
            int next_exam_index = exam->exam_index + 1;
            if (!load_exam(exam, next_exam_index)) {
                std::cout << "[TA " << ta_id << "] No more exams to load" << std::endl;
                break;
            }
            std::cout << "[TA " << ta_id << "] Loaded exam for student " 
                      << exam->student_number << std::endl;
        }
        
        // If didn't mark anything (all questions taken), brief pause before trying again
        if (!marked_something) {
            usleep(100000);  // 0.1 second
        }
    }
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_TAs>" << std::endl;
        return 1;
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        std::cerr << "Error: Number of TAs must be at least 2" << std::endl;
        return 1;
    }
    
    std::cout << "Starting TA marking system with " << num_tas << " TAs" << std::endl;
    
    // Create shared memory for rubric
    int shm_rubric_id = shmget(IPC_PRIVATE, sizeof(Rubric), IPC_CREAT | 0666);
    if (shm_rubric_id < 0) {
        std::cerr << "Error: Failed to create shared memory for rubric" << std::endl;
        return 1;
    }
    
    Rubric* rubric = (Rubric*)shmat(shm_rubric_id, NULL, 0);
    if (rubric == (void*)-1) {
        std::cerr << "Error: Failed to attach shared memory for rubric" << std::endl;
        return 1;
    }
    
    // Create shared memory for current exam
    int shm_exam_id = shmget(IPC_PRIVATE, sizeof(CurrentExam), IPC_CREAT | 0666);
    if (shm_exam_id < 0) {
        std::cerr << "Error: Failed to create shared memory for exam" << std::endl;
        return 1;
    }
    
    CurrentExam* exam = (CurrentExam*)shmat(shm_exam_id, NULL, 0);
    if (exam == (void*)-1) {
        std::cerr << "Error: Failed to attach shared memory for exam" << std::endl;
        return 1;
    }
    
    // Load initial rubric
    load_rubric(rubric);
    std::cout << "Loaded rubric into shared memory" << std::endl;
    
    // Load first exam
    if (!load_exam(exam, 1)) {
        std::cerr << "Error: Could not load first exam (exam_0001.txt)" << std::endl;
        return 1;
    }
    std::cout << "Loaded first exam (student " << exam->student_number << ")" << std::endl;
    
    // Create TA processes
    std::vector<pid_t> ta_pids;
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Error: Failed to fork TA process " << i << std::endl;
            return 1;
        } else if (pid == 0) {
            // Child process (TA)
            ta_process(i + 1, rubric, exam);
            exit(0);
        } else {
            // Parent process
            ta_pids.push_back(pid);
        }
    }
    
    // Wait for all TA processes to finish
    for (pid_t pid : ta_pids) {
        waitpid(pid, NULL, 0);
    }
    
    std::cout << "All TAs have finished marking" << std::endl;
    
    // Cleanup shared memory
    shmdt(rubric);
    shmdt(exam);
    shmctl(shm_rubric_id, IPC_RMID, NULL);
    shmctl(shm_exam_id, IPC_RMID, NULL);
    
    return 0;
}
