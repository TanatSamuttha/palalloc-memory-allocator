#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include "palalloc.h"

struct Obj16  { uint8_t data[16];  }; // Class 0
struct Obj32  { uint8_t data[32];  }; // Class 1
struct Obj64  { uint8_t data[64];  }; // Class 2
struct Obj128 { uint8_t data[128]; }; // Class 3

const int TOTAL_OBJECTS = 500000;
const int POOL_PAGES = 20000; // ~80MB pool to ensure we don't run out of space
const int MAX_SIZE = 128;

struct ObjectData {
    int size_class;
    bool survives;
};

std::vector<ObjectData> scenario;

void generateScenario() {
    scenario.clear();
    scenario.reserve(TOTAL_OBJECTS);

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist_size(0, 3);
    std::uniform_real_distribution<double> dist_prob(0.0, 1.0);

    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        ObjectData data;
        data.size_class = dist_size(rng);
        data.survives = (dist_prob(rng) >= 0.95); // Only 5% survive
        scenario.push_back(data);
    }
}

// ---------------------------------------------------------
// [Fix for -O3] Global volatile sink to prevent dead-code elimination
// ---------------------------------------------------------
volatile uint8_t global_sink = 0;

double benchmarkMallocCompaction() {
    std::vector<void*> active_ptrs(TOTAL_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0; // Accumulator to trick compiler

    // -----------------------------------------------------------------
    // 1. Fill Phase
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        size_t alloc_size = 0;
        switch (scenario[i].size_class) {
            case 0: alloc_size = sizeof(Obj16); break;
            case 1: alloc_size = sizeof(Obj32); break;
            case 2: alloc_size = sizeof(Obj64); break;
            case 3: alloc_size = sizeof(Obj128); break;
        }
        
        active_ptrs[i] = std::malloc(alloc_size);
        if (active_ptrs[i]) {
            std::memset(active_ptrs[i], 0xAA, 16); // Dummy data
        }
    }

    // -----------------------------------------------------------------
    // 2. Swiss Cheese Phase (Free 95%)
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        if (!scenario[i].survives && active_ptrs[i]) {
            std::free(active_ptrs[i]);
            active_ptrs[i] = nullptr; // Mark as hole
        }
    }

    // -----------------------------------------------------------------
    // 3. Compaction Phase (Manual Re-allocation & Copy)
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        if (scenario[i].survives && active_ptrs[i]) {
            size_t alloc_size = 0;
            switch (scenario[i].size_class) {
                case 0: alloc_size = sizeof(Obj16); break;
                case 1: alloc_size = sizeof(Obj32); break;
                case 2: alloc_size = sizeof(Obj64); break;
                case 3: alloc_size = sizeof(Obj128); break;
            }
            
            void* new_ptr = std::malloc(alloc_size);
            if (new_ptr) {
                std::memcpy(new_ptr, active_ptrs[i], alloc_size); // Move data
                
                // [Fix] Read from the newly compacted memory location to prevent optimization
                local_sum += static_cast<uint8_t*>(new_ptr)[0];
            }
            
            std::free(active_ptrs[i]); // Free the old fragmented location
            active_ptrs[i] = new_ptr;  // Update reference
        }
    }

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();

    // Cleanup for ASAN/Valgrind (not timed)
    for (void* ptr : active_ptrs) {
        if (ptr) std::free(ptr);
    }

    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalallocCompaction() {
    Palalloc pool_A(POOL_PAGES, MAX_SIZE);
    Palalloc pool_B(POOL_PAGES, MAX_SIZE);
    pool_A.init();
    pool_B.init();

    Palalloc* active_pool = &pool_A;
    Palalloc* standby_pool = &pool_B;

    std::vector<void*> active_ptrs(TOTAL_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0; // Accumulator to trick compiler

    // -----------------------------------------------------------------
    // 1. Fill Phase
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        switch (scenario[i].size_class) {
            case 0: active_ptrs[i] = active_pool->galloc<Obj16>(); break;
            case 1: active_ptrs[i] = active_pool->galloc<Obj32>(); break;
            case 2: active_ptrs[i] = active_pool->galloc<Obj64>(); break;
            case 3: active_ptrs[i] = active_pool->galloc<Obj128>(); break;
        }
        
        if (active_ptrs[i]) {
            std::memset(active_ptrs[i], 0xAA, 16);
        }
    }

    // -----------------------------------------------------------------
    // 2. Swiss Cheese Phase (Free 95%)
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        if (!scenario[i].survives && active_ptrs[i]) {
            switch (scenario[i].size_class) {
                case 0: active_pool->free(static_cast<Obj16*>(active_ptrs[i])); break;
                case 1: active_pool->free(static_cast<Obj32*>(active_ptrs[i])); break;
                case 2: active_pool->free(static_cast<Obj64*>(active_ptrs[i])); break;
                case 3: active_pool->free(static_cast<Obj128*>(active_ptrs[i])); break;
            }
            active_ptrs[i] = nullptr;
        }
    }

    // -----------------------------------------------------------------
    // 3. Compaction Phase (Migrate to Standby Pool & Hard Reset)
    // -----------------------------------------------------------------
    for (int i = 0; i < TOTAL_OBJECTS; ++i) {
        if (scenario[i].survives && active_ptrs[i]) {
            void* new_ptr = nullptr;
            size_t copy_size = 0;

            switch (scenario[i].size_class) {
                case 0: new_ptr = standby_pool->galloc<Obj16>(); copy_size = 16; break;
                case 1: new_ptr = standby_pool->galloc<Obj32>(); copy_size = 32; break;
                case 2: new_ptr = standby_pool->galloc<Obj64>(); copy_size = 64; break;
                case 3: new_ptr = standby_pool->galloc<Obj128>(); copy_size = 128; break;
            }

            if (new_ptr) {
                std::memcpy(new_ptr, active_ptrs[i], copy_size);
                
                // [Fix] Read from the newly compacted memory location to prevent optimization
                local_sum += static_cast<uint8_t*>(new_ptr)[0];
            }
            
            active_ptrs[i] = new_ptr; // Update reference
        }
    }

    // Nuke the old pool in O(1) time
    active_pool->hardReset();
    std::swap(active_pool, standby_pool);

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Benchmark Scenario: Swiss Cheese & Memory Compaction\n";
    std::cout << " Simulating fragmentation and O(1) hard reset with 2 Pools\n";
    std::cout << " Total Objects: " << TOTAL_OBJECTS << "\n";
    std::cout << " Generating Scenario (Please Wait)...\n";
    
    generateScenario();

    std::cout << " Starting Benchmark...\n";
    std::cout << "========================================================\n\n";

    double malloc_time = benchmarkMallocCompaction();
    double palalloc_time = benchmarkPalallocCompaction();
    double speedup = malloc_time / palalloc_time;

    std::cout << "[std::malloc] Total Time: " 
              << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
              
    std::cout << "[Palalloc]    Total Time: " 
              << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    std::cout << ">>> Full lifecycle (Fill -> Fragment -> Compact) speedup:\n";
    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) << speedup << "x faster than std::malloc\n";
              
    std::cout << "========================================================\n";

    return 0;
}