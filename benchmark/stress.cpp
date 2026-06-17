#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "palalloc.h"

// Define structures for the extreme ends of our size classes
struct ObjSmall { uint8_t data[256];  }; // Class 0
struct ObjLarge { uint8_t data[2048]; }; // Class 3

// Pool calculation:
// We want to test the boundaries. Let's use a 40 MB pool (~10,000 pages).
// Class 0 (256B) normally gets ~20 MB (max 80,000 objects).
// Class 3 (2048B) normally gets ~5 MB (max 2,500 objects).
const int POOL_PAGES = 10000; 
const int MAX_SIZE_CLASS = 2048;

// We will deliberately overallocate beyond the static partition boundaries
const int BURST_LARGE_COUNT = 12000; // Requires ~24 MB (Forces heavy COMBINE)
const int BURST_SMALL_COUNT = 100000; // Requires ~25 MB (Forces heavy SPLIT)
const int CYCLE_COUNT = 500; // Number of times we oscillate between extremes

double benchmarkMalloc() {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    // Reserve max capacity to avoid std::vector allocation overhead during benchmark
    ptrs.reserve(std::max(BURST_LARGE_COUNT, BURST_SMALL_COUNT));

    for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
        // Phase 1: Heavy allocation of large objects
        for (int i = 0; i < BURST_LARGE_COUNT; ++i) {
            void* ptr = std::malloc(sizeof(ObjLarge));
            static_cast<uint8_t*>(ptr)[0] = 0xAA; // Force commit
            ptrs.push_back(ptr);
        }

        // Free large objects
        for (void* ptr : ptrs) {
            std::free(ptr);
        }
        ptrs.clear();

        // Phase 2: Heavy allocation of small objects
        for (int i = 0; i < BURST_SMALL_COUNT; ++i) {
            void* ptr = std::malloc(sizeof(ObjSmall));
            static_cast<uint8_t*>(ptr)[0] = 0xBB; // Force commit
            ptrs.push_back(ptr);
        }

        // Free small objects
        for (void* ptr : ptrs) {
            std::free(ptr);
        }
        ptrs.clear();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalalloc() {
    // Initialize Palalloc with 10,000 pages (~40MB) and 2048B max size class
    Palalloc allocator(POOL_PAGES, MAX_SIZE_CLASS);
    allocator.init();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<void*> ptrs;
    ptrs.reserve(std::max(BURST_LARGE_COUNT, BURST_SMALL_COUNT));

    for (int cycle = 0; cycle < CYCLE_COUNT; ++cycle) {
        // Phase 1: Heavy allocation of large objects
        // This will quickly exhaust virgin[3] (which only holds ~2,500 objects).
        // Palalloc will be FORCED to use combine() to steal tail space from smaller classes.
        for (int i = 0; i < BURST_LARGE_COUNT; ++i) {
            void* ptr = allocator.galloc<ObjLarge>();
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xAA;
                ptrs.push_back(ptr);
            }
        }

        // Free large objects - they go into head[3]
        for (void* ptr : ptrs) {
            allocator.free(static_cast<ObjLarge*>(ptr));
        }
        ptrs.clear();

        // Phase 2: Heavy allocation of small objects
        // This will exhaust virgin[0]. Since combine() previously ate the tail space,
        // and head[0] is empty, Palalloc will be FORCED to use split() recursively
        // to break down the large blocks currently sitting in head[3] or larger tails.
        for (int i = 0; i < BURST_SMALL_COUNT; ++i) {
            void* ptr = allocator.galloc<ObjSmall>();
            if (ptr) {
                static_cast<uint8_t*>(ptr)[0] = 0xBB;
                ptrs.push_back(ptr);
            }
        }

        // Free small objects - they go into head[0]
        for (void* ptr : ptrs) {
            allocator.free(static_cast<ObjSmall*>(ptr));
        }
        ptrs.clear();
    }

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