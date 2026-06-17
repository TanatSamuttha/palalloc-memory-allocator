#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "palalloc.h" 

// Mock structure representing in-game data (16 Bytes)
struct Transform {
    float x, y, z;
    uint32_t id;
};

const int NUM_FRAMES = 1000;
const int ALLOCS_PER_FRAME = 50000;

// Arrays to store pointers, preventing the compiler from optimizing the allocations away
Transform* malloc_ptrs[ALLOCS_PER_FRAME];
Transform* palalloc_ptrs[ALLOCS_PER_FRAME];

double benchmarkMalloc() {
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        // 1. Simulate memory allocation during the frame
        for (int i = 0; i < ALLOCS_PER_FRAME; ++i) {
            malloc_ptrs[i] = static_cast<Transform*>(std::malloc(sizeof(Transform)));
            
            // Write data to force memory page faults and simulate cache usage
            malloc_ptrs[i]->x = 1.0f;
            malloc_ptrs[i]->y = 2.0f;
            malloc_ptrs[i]->z = 3.0f;
            malloc_ptrs[i]->id = i;
        }

        // 2. Simulate freeing memory at the end of the frame
        for (int i = 0; i < ALLOCS_PER_FRAME; ++i) {
            std::free(malloc_ptrs[i]);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    
    return elapsed.count();
}

double benchmarkPalalloc() {
    /* Calculate Pool Size: 
       We need to allocate 50,000 items of 16 Bytes = 800,000 Bytes.
       If maxSize = 64, the 16 Bytes Size Class gets 1/4 of the pool area.
       Therefore, the pool must be at least 3.2 MB (approx. 800 pages).
       We allocate 1000 pages (approx. 4 MB) to provide enough headroom.
    */
    Palalloc allocator(1000, 64);
    allocator.init(); // Initialize before measuring time to avoid overhead

    auto start_time = std::chrono::high_resolution_clock::now();
    int total_failures = 0;

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
        // 1. Simulate memory allocation during the frame
        for (int i = 0; i < ALLOCS_PER_FRAME; ++i) {
            palalloc_ptrs[i] = allocator.alloc<Transform>();
            
            // Write data similar to the malloc test
            if (palalloc_ptrs[i] != nullptr) {
                palalloc_ptrs[i]->x = 1.0f;
                palalloc_ptrs[i]->y = 2.0f;
                palalloc_ptrs[i]->z = 3.0f;
                palalloc_ptrs[i]->id = i;
            } else {
                total_failures++;
            }
        }

        // 2. Simulate freeing memory at the end of the frame
        // This highlights Palalloc's strength: O(1) Reset clears everything in a single operation
        allocator.reset();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    
    if (total_failures > 0) {
        std::cout << "[Warning] Palalloc encountered " << total_failures << " allocation failures!\n";
    }
    
    return elapsed.count();
}

int main() {
    std::cout << "=========================================\n";
    std::cout << " Benchmark Scenario 1: Game Loop / Reset \n";
    std::cout << " Frames: " << NUM_FRAMES << " | Allocs/Frame: " << ALLOCS_PER_FRAME << "\n";
    std::cout << " Total Allocations: " << (NUM_FRAMES * ALLOCS_PER_FRAME) << "\n";
    std::cout << "=========================================\n\n";

    // Run benchmarks
    double malloc_time = benchmarkMalloc();
    double palalloc_time = benchmarkPalalloc();

    // Calculate how many times faster Palalloc is
    double speedup = malloc_time / palalloc_time;

    // Display results
    std::cout << "[std::malloc] Total Time: " 
              << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
              
    std::cout << "[Palalloc]    Total Time: " 
              << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) 
              << speedup << "x faster than std::malloc!\n";
              
    std::cout << "=========================================\n";

    return 0;
}