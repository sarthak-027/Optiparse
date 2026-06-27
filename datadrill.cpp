#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <regex> // NEW: For advanced pattern matching

std::atomic<int> global_match_count(0);
std::mutex write_mutex;
std::ofstream output_file;

void processChunk(std::string filename, uintmax_t start, uintmax_t end, std::string target_pattern, int thread_id) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return;

    file.seekg(start);
    std::string line;

    if (start != 0) std::getline(file, line);

    int local_count = 0;
    
    // NEW: Initialize regex pattern
    std::regex pattern(target_pattern);
    
    // NEW: The Batch Buffer. Holds 1000 lines in RAM before writing to the SSD.
    std::vector<std::string> batch_buffer;
    batch_buffer.reserve(1000); 

    while (file.tellg() != -1 && file.tellg() < end) {
        // NEW: Grab the exact byte address before reading the line
        uintmax_t exact_byte_position = file.tellg(); 

        if (std::getline(file, line)) {
            if (std::regex_search(line, pattern)) {
                local_count++;
                
                // NEW: Prepend the Byte Offset to the extracted data
                std::string formatted_line = "[Byte Offset: " + std::to_string(exact_byte_position) + "] " + line;
                batch_buffer.push_back(formatted_line);

                if (batch_buffer.size() >= 1000) {
                    std::lock_guard<std::mutex> lock(write_mutex);
                    for (const auto& match : batch_buffer) {
                        output_file << match << "\n";
                    }
                    batch_buffer.clear();
                }
            }
        }
    }
    // Dump any remaining lines in the buffer before the thread dies
    if (!batch_buffer.empty()) {
        std::lock_guard<std::mutex> lock(write_mutex);
        for (const auto& match : batch_buffer) {
            output_file << match << "\n";
        }
    }

    global_match_count += local_count;
    
    // Live update sent to Node.js!
    std::cout << "[Thread " << thread_id << "] Sector cleared. Anomalies found: " << local_count << std::endl;
}

int main(int argc, char* argv[]) {
    // Note: We use std::endl to force flushing the output instantly to WebSockets
    std::cout << "\n\033[1;34m============================================\033[0m" << std::endl;
    std::cout << "\033[1;37m  OptiParse v3.0 - Autonomous Pipeline  \033[0m" << std::endl;
    std::cout << "\033[1;34m============================================\033[0m\n" << std::endl;

    if (argc < 3) return 1;

    std::string filename = argv[1];
    std::string target = argv[2];
    std::string output_name = "extracted_data.txt";

    if (!std::filesystem::exists(filename)) return 1;

    output_file.open(output_name);

    uintmax_t file_size = std::filesystem::file_size(filename);
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Target Pattern (Regex): '" << target << "'" << std::endl;
    std::cout << "Hardware Threads: " << num_threads << std::endl;
    std::cout << "Streaming data... \n" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    uintmax_t chunk_size = file_size / num_threads;

    for (unsigned int i = 0; i < num_threads; ++i) {
        uintmax_t start = i * chunk_size;
        uintmax_t end = (i == num_threads - 1) ? file_size : start + chunk_size;
        threads.emplace_back(processChunk, filename, start, end, target, i + 1);
    }

    for (auto& t : threads) {
        t.join();
    }

    output_file.close(); 

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::cout << "\n\033[1;32m============================================\033[0m" << std::endl;
    std::cout << ">> TOTAL ENTRIES EXTRACTED: \033[1;31m" << global_match_count << "\033[0m" << std::endl;
    std::cout << ">> EXECUTION TIME: \033[1;33m" << duration.count() << " seconds\033[0m" << std::endl;
    std::cout << "\033[1;32m============================================\033[0m\n" << std::endl;

    return 0;
}