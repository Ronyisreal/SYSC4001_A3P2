/**
 * @file ta_marking_semaphore.cpp
 * @brief Assignment 3 Part 2b - Concurrent TA Marking System with Semaphores
 * @author Student Implementation
 * 
 * This program extends Part 2a by adding semaphore-based synchronization
 * to eliminate race conditions and ensure proper coordination between TAs.
 * 
 * Compile: g++ -o ta_marking_semaphore ta_marking_semaphore.cpp
 * Run: ./ta_marking_semaphore <number_of_TAs>
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
#include <sys/sem.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <iomanip>

// Constants
#define NUM_EXERCISES 5
#define RUBRIC_FILE "rubric.txt"
#define EXAM_PREFIX "exam_"
#define EXAM_SUFFIX ".txt"

// Semaphore indices
#define SEM_RUBRIC_MUTEX 0      // Mutex for rubric write access
#define SEM_RUBRIC_READERS 1    // Reader count for rubric
#define SEM_EXAM_MUTEX 2        // Mutex for exam marking coordination
#define SEM_EXAM_LOADING 3      // Mutex for loading next exam
#define NUM_SEMAPHORES 4

// Union for semaphore operations (required for some systems)
#if defined(__APPLE__) || defined(__FreeBSD__)
// macOS and FreeBSD already define semun
#else
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

// Shared memory structure for rubric
struct Rubric {
    char exercises[NUM_EXERCISES][100];
    int reader_count;  // Track number of readers
};

// Shared memory structure for current exam
struct CurrentExam {
    int student_number;
    bool questions_marked[NUM_EXERCISES];
    int exam_index;
    bool being_marked[NUM_EXERCISES];  // Track which questions are being marked
};

// Semaphore operation helper functions
void sem_wait(int semid, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = -1;  // Wait (decrement)
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("sem_wait failed");
        exit(1);
    }
}

void sem_signal(int semid, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = 1;  // Signal (increment)
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("sem_signal failed");
        exit(1);
    }
}

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
            exam->being_marked[i] = false;
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

// Reader-Writer pattern for rubric access
void rubric_read_lock(int semid, Rubric* rubric, int ta_id) {
    std::cout << "[TA " << ta_id << "] REQUESTING rubric read access" << std::endl;
    
    // Entry section for readers
    sem_wait(semid, SEM_RUBRIC_READERS);  // Protect reader count
    rubric->reader_count++;
    if (rubric->reader_count == 1) {
        // First reader locks out writers
        sem_wait(semid, SEM_RUBRIC_MUTEX);
        std::cout << "[TA " << ta_id << "] LOCKED rubric for reading (first reader)" << std::endl;
    } else {
        std::cout << "[TA " << ta_id << "] JOINED rubric reading (reader #" 
                  << rubric->reader_count << ")" << std::endl;
    }
    sem_signal(semid, SEM_RUBRIC_READERS);  // Release reader count protection
}

void rubric_read_unlock(int semid, Rubric* rubric, int ta_id) {
    // Exit section for readers
    sem_wait(semid, SEM_RUBRIC_READERS);  // Protect reader count
    rubric->reader_count--;
    std::cout << "[TA " << ta_id << "] RELEASED rubric read access (readers remaining: " 
              << rubric->reader_count << ")" << std::endl;
    if (rubric->reader_count == 0) {
        // Last reader unlocks writers
        sem_signal(semid, SEM_RUBRIC_MUTEX);
        std::cout << "[TA " << ta_id << "] UNLOCKED rubric (last reader)" << std::endl;
    }
    sem_signal(semid, SEM_RUBRIC_READERS);  // Release reader count protection
}

void rubric_write_lock(int semid, int ta_id) {
    std::cout << "[TA " << ta_id << "] REQUESTING rubric write access" << std::endl;
    sem_wait(semid, SEM_RUBRIC_MUTEX);  // Exclusive access
    std::cout << "[TA " << ta_id << "] ACQUIRED rubric write lock" << std::endl;
}

void rubric_write_unlock(int semid, int ta_id) {
    std::cout << "[TA " << ta_id << "] RELEASING rubric write lock" << std::endl;
    sem_signal(semid, SEM_RUBRIC_MUTEX);
}

// TA process function with semaphore synchronization
void ta_process(int ta_id, Rubric* rubric, CurrentExam* exam, int semid) {
    srand(time(NULL) + ta_id);
    
    std::cout << "[TA " << ta_id << "] ===== STARTED WORKING =====" << std::endl;
    
    while (true) {
        // Check if we've reached the end (student 9999)
        // This check needs to be protected
        sem_wait(semid, SEM_EXAM_MUTEX);
        int current_student = exam->student_number;
        sem_signal(semid, SEM_EXAM_MUTEX);
        
        if (current_student == 9999) {
            std::cout << "[TA " << ta_id << "] ===== FINISHED - reached student 9999 =====" << std::endl;
            break;
        }
        
        std::cout << "[TA " << ta_id << "] >>> STARTING rubric review for exam " 
                  << current_student << std::endl;
        
        // CRITICAL SECTION: Review rubric (readers can read concurrently)
        rubric_read_lock(semid, rubric, ta_id);
        
        std::cout << "[TA " << ta_id << "] ENTERED rubric read critical section" << std::endl;
        
        // Review each exercise in the rubric
        for (int i = 0; i < NUM_EXERCISES; i++) {
            random_delay(0.5, 1.0);
            
            // Randomly decide if rubric needs correction (30% chance)
            if ((rand() % 100) < 30) {
                // Need to upgrade from read to write lock
                rubric_read_unlock(semid, rubric, ta_id);
                
                std::cout << "[TA " << ta_id << "] DETECTED error in rubric exercise " 
                          << (i + 1) << ", upgrading to write lock" << std::endl;
                
                // CRITICAL SECTION: Write to rubric (exclusive access)
                rubric_write_lock(semid, ta_id);
                
                std::cout << "[TA " << ta_id << "] ENTERED rubric write critical section" << std::endl;
                
                char* rubric_line = rubric->exercises[i];
                char* comma = strchr(rubric_line, ',');
                if (comma != NULL && *(comma + 1) == ' ') {
                    char& rubric_char = *(comma + 2);
                    std::cout << "[TA " << ta_id << "] WRITING: Changing exercise " << (i + 1) 
                              << " rubric from '" << rubric_char << "' to '" 
                              << (char)(rubric_char + 1) << "'" << std::endl;
                    rubric_char++;
                    
                    save_rubric(rubric);
                    std::cout << "[TA " << ta_id << "] SAVED rubric to file" << std::endl;
                }
                
                rubric_write_unlock(semid, ta_id);
                std::cout << "[TA " << ta_id << "] EXITED rubric write critical section" << std::endl;
                
                // Re-acquire read lock to continue review
                rubric_read_lock(semid, rubric, ta_id);
            }
        }
        
        rubric_read_unlock(semid, rubric, ta_id);
        std::cout << "[TA " << ta_id << "] EXITED rubric read critical section" << std::endl;
        std::cout << "[TA " << ta_id << "] <<< COMPLETED rubric review" << std::endl;
        
        // CRITICAL SECTION: Mark a question
        std::cout << "[TA " << ta_id << "] >>> ATTEMPTING to mark a question" << std::endl;
        
        bool marked_something = false;
        
        sem_wait(semid, SEM_EXAM_MUTEX);  // Protect exam access
        std::cout << "[TA " << ta_id << "] ENTERED exam marking critical section" << std::endl;
        
        for (int q = 0; q < NUM_EXERCISES; q++) {
            if (!exam->questions_marked[q] && !exam->being_marked[q]) {
                // Mark this question as being worked on
                exam->being_marked[q] = true;
                marked_something = true;
                
                int student = exam->student_number;
                
                std::cout << "[TA " << ta_id << "] CLAIMED question " << (q + 1) 
                          << " of student " << student << std::endl;
                
                sem_signal(semid, SEM_EXAM_MUTEX);  // Release lock while marking
                
                // Marking takes time (outside critical section)
                std::cout << "[TA " << ta_id << "] MARKING student " << student 
                          << ", question " << (q + 1) << " (this will take 1-2 seconds)" << std::endl;
                random_delay(1.0, 2.0);
                
                // Re-acquire lock to update status
                sem_wait(semid, SEM_EXAM_MUTEX);
                exam->questions_marked[q] = true;
                exam->being_marked[q] = false;
                
                std::cout << "[TA " << ta_id << "] COMPLETED marking student " << student 
                          << ", question " << (q + 1) << std::endl;
                
                sem_signal(semid, SEM_EXAM_MUTEX);
                break;
            }
        }
        
        if (!marked_something) {
            sem_signal(semid, SEM_EXAM_MUTEX);
        }
        
        std::cout << "[TA " << ta_id << "] EXITED exam marking critical section" << std::endl;
        
        if (!marked_something) {
            // Check if all questions are marked
            sem_wait(semid, SEM_EXAM_MUTEX);
            
            if (all_questions_marked(exam)) {
                std::cout << "[TA " << ta_id << "] DETECTED all questions marked for student " 
                          << exam->student_number << std::endl;
                
                // CRITICAL SECTION: Load next exam (only one TA should do this)
                sem_signal(semid, SEM_EXAM_MUTEX);
                
                sem_wait(semid, SEM_EXAM_LOADING);  // Exclusive access for loading
                std::cout << "[TA " << ta_id << "] ENTERED exam loading critical section" << std::endl;
                
                // Double-check after acquiring lock
                sem_wait(semid, SEM_EXAM_MUTEX);
                if (all_questions_marked(exam)) {
                    int old_student = exam->student_number;
                    int next_exam_index = exam->exam_index + 1;
                    
                    std::cout << "[TA " << ta_id << "] LOADING next exam (index " 
                              << next_exam_index << ")..." << std::endl;
                    
                    if (!load_exam(exam, next_exam_index)) {
                        std::cout << "[TA " << ta_id << "] No more exams to load" << std::endl;
                        sem_signal(semid, SEM_EXAM_MUTEX);
                        sem_signal(semid, SEM_EXAM_LOADING);
                        break;
                    }
                    
                    std::cout << "[TA " << ta_id << "] LOADED exam for student " 
                              << exam->student_number << " (was " << old_student << ")" << std::endl;
                }
                sem_signal(semid, SEM_EXAM_MUTEX);
                
                sem_signal(semid, SEM_EXAM_LOADING);
                std::cout << "[TA " << ta_id << "] EXITED exam loading critical section" << std::endl;
            } else {
                sem_signal(semid, SEM_EXAM_MUTEX);
                // Brief pause before trying again
                usleep(100000);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_TAs>" << std::endl;
        return 1;
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        std::cerr << "Error: Number of TAs must be at least 2" << std::endl;
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "Starting TA marking system with " << num_tas << " TAs" << std::endl;
    std::cout << "WITH SEMAPHORE SYNCHRONIZATION" << std::endl;
    std::cout << "========================================" << std::endl;
    
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
    
    // Create semaphore set
    int semid = semget(IPC_PRIVATE, NUM_SEMAPHORES, IPC_CREAT | 0666);
    if (semid < 0) {
        std::cerr << "Error: Failed to create semaphores" << std::endl;
        return 1;
    }
    
    // Initialize semaphores
    union semun arg;
    arg.val = 1;
    semctl(semid, SEM_RUBRIC_MUTEX, SETVAL, arg);     // Binary semaphore for rubric write
    semctl(semid, SEM_RUBRIC_READERS, SETVAL, arg);   // Binary semaphore for reader count
    semctl(semid, SEM_EXAM_MUTEX, SETVAL, arg);       // Binary semaphore for exam access
    semctl(semid, SEM_EXAM_LOADING, SETVAL, arg);     // Binary semaphore for exam loading
    
    std::cout << "Semaphores initialized" << std::endl;
    
    // Initialize rubric reader count
    rubric->reader_count = 0;
    
    // Load initial rubric
    load_rubric(rubric);
    std::cout << "Loaded rubric into shared memory" << std::endl;
    
    // Load first exam
    if (!load_exam(exam, 1)) {
        std::cerr << "Error: Could not load first exam (exam_0001.txt)" << std::endl;
        return 1;
    }
    std::cout << "Loaded first exam (student " << exam->student_number << ")" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    // Create TA processes
    std::vector<pid_t> ta_pids;
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Error: Failed to fork TA process " << i << std::endl;
            return 1;
        } else if (pid == 0) {
            // Child process (TA)
            ta_process(i + 1, rubric, exam, semid);
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
    
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "All TAs have finished marking" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Cleanup
    shmdt(rubric);
    shmdt(exam);
    shmctl(shm_rubric_id, IPC_RMID, NULL);
    shmctl(shm_exam_id, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    
    std::cout << "Cleaned up shared memory and semaphores" << std::endl;
    
    return 0;
}
