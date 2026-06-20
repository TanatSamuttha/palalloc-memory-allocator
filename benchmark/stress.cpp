#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <algorithm>
#include "palalloc.h"

// Define structures for the extreme ends of our size classes
struct ObjSmall { uint8_t data[256];  }; // Class 0
struct ObjLarge { uint8_t data[2048]; }; // Class 3

const int POOL_PAGES = 10000; 
const int MAX_SIZE_CLASS = 2048;

const int BURST_LARGE_COUNT = 12000; // Requires ~24 MB (Forces heavy COMBINE)
const int BURST_SMALL_COUNT = 100000; // Requires ~25 MB (Forces heavy SPLIT)
const int CYCLE_COUNT = 500; // Number of times we oscillate between extremes

// ---------------------------------------------------------
// [Fix for -O3] Global volatile sink to prevent dead-code elimination
// ---------------------------------------------------------
volatile uint8_t global_sink = 0;

double benchmarkMalloc() {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    ptrs.reserve(std::max(BURST_LARGE_COUNT, BURST_SMALL_COUNT));

    uint8_t local_sum = 0; // Accumulator to trick compiler

    for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
        // Phase 1: Heavy allocation of large objects
        for (int i = 0; i < BURST_LARGE_COUNT; ++i) {
            void* ptr = std::malloc(sizeof(ObjLarge));
            // [Fix] Add nullptr check for safety and read back the value
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xAA; // Force commit
                local_sum += static_cast<uint8_t*>(ptr)[0]; // [Fix] Prevent optimization
                ptrs.push_back(ptr);
            }
        }

        // Free large objects
        for (void* ptr : ptrs) {
            std::free(ptr);
        }
        ptrs.clear();

        // Phase 2: Heavy allocation of small objects
        for (int i = 0; i < BURST_SMALL_COUNT; ++i) {
            void* ptr = std::malloc(sizeof(ObjSmall));
            // [Fix] Add nullptr check for safety and read back the value
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xBB; // Force commit
                local_sum += static_cast<uint8_t*>(ptr)[0]; // [Fix] Prevent optimization
                ptrs.push_back(ptr);
            }
        }

        // Free small objects
        for (void* ptr : ptrs) {
            std::free(ptr);
        }
        ptrs.clear();
    }

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalalloc() {
    Palalloc allocator(POOL_PAGES, MAX_SIZE_CLASS);
    allocator.init();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    ptrs.reserve(std::max(BURST_LARGE_COUNT, BURST_SMALL_COUNT));

    uint8_t local_sum = 0; // Accumulator to trick compiler

    for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
        // Phase 1: Heavy allocation of large objects
        for (int i = 0; i < BURST_LARGE_COUNT; ++i) {
            void* ptr = allocator.galloc<ObjLarge>();
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xAA;
                local_sum += static_cast<uint8_t*>(ptr)[0]; // [Fix] Prevent optimization
                ptrs.push_back(ptr);
            }
        }

        // Free large objects
        for (void* ptr : ptrs) {
            allocator.free(static_cast<ObjLarge*>(ptr));
        }
        ptrs.clear();

        // Phase 2: Heavy allocation of small objects
        for (int i = 0; i < BURST_SMALL_COUNT; ++i) {
            void* ptr = allocator.galloc<ObjSmall>();
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xBB;
                local_sum += static_cast<uint8_t*>(ptr)[0]; // [Fix] Prevent optimization
                ptrs.push_back(ptr);
            }
        }

        // Free small objects
        for (void* ptr : ptrs) {
            allocator.free(static_cast<ObjSmall*>(ptr));
        }
        ptrs.clear();
    }

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Benchmark Scenario 4: The Oscillating Stress Test\n";
    std::cout << " Forcing heavy usage of internal combine() and split()\n";
    std::cout << " Cycles: " << CYCLE_COUNT << "\n";
    std::cout << " Large Burst: " << BURST_LARGE_COUNT << " objects (2048B)\n";
    std::cout << " Small Burst: " << BURST_SMALL_COUNT << " objects (256B)\n";
    std::cout << "========================================================\n\n";

    double malloc_time = benchmarkMalloc();
    double palalloc_time = benchmarkPalalloc();
    double speedup = malloc_time / palalloc_time;

    std::cout << "[std::malloc] Total Time: " 
              << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
              
    std::cout << "[Palalloc]    Total Time: " 
              << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    std::cout << ">>> Under extreme combine/split pressure,\n";
    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) << speedup << "x faster than std::malloc!\n";
              
    std::cout << "========================================================\n";

    return 0;
}