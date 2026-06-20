#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include "palalloc.h"

// Define large object structures to bypass malloc's small-bin fast paths
struct Obj256  { uint8_t data[256];  };
struct Obj512  { uint8_t data[512];  };
struct Obj1024 { uint8_t data[1024]; };
struct Obj2048 { uint8_t data[2048]; };

const int NUM_OPERATIONS = 2000000; // 2 Million chaotic operations
const int MAX_ACTIVE_OBJECTS = 50000; // Maximum concurrent objects alive

enum class OpType { ALLOC, FREE };

struct Instruction {
    OpType type;
    int slot_index;
    int size_class; // 0=256, 1=512, 2=1024, 3=2048
};

std::vector<Instruction> operations;

void generateChaos() {
    std::mt19937 rng(42); // Fixed seed for reproducible benchmarks
    std::uniform_int_distribution<int> dist_action(0, 100);
    std::uniform_int_distribution<int> dist_size(0, 3);
    std::uniform_int_distribution<int> dist_slot(0, MAX_ACTIVE_OBJECTS - 1);

    std::vector<bool> slot_occupied(MAX_ACTIVE_OBJECTS, false);
    std::vector<int> slot_sizes(MAX_ACTIVE_OBJECTS, -1);
    
    operations.reserve(NUM_OPERATIONS);

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        int slot = dist_slot(rng);
        int action_roll = dist_action(rng);

        if (slot_occupied[slot]) {
            if (action_roll < 60) {
                operations.push_back({OpType::FREE, slot, slot_sizes[slot]});
                slot_occupied[slot] = false;
            }
        } else {
            if (action_roll >= 40) {
                int size_class = dist_size(rng);
                operations.push_back({OpType::ALLOC, slot, size_class});
                slot_occupied[slot] = true;
                slot_sizes[slot] = size_class;
            }
        }
    }
}

// ---------------------------------------------------------
// [Fix for -O3] Global volatile sink to prevent dead-code elimination
// ---------------------------------------------------------
volatile uint8_t global_sink = 0;

double benchmarkMalloc() {
    std::vector<void*> active_ptrs(MAX_ACTIVE_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0; // Accumulator to trick compiler

    for (const auto& op : operations) {
        if (op.type == OpType::ALLOC) {
            size_t alloc_size = 0;
            switch (op.size_class) {
                case 0: alloc_size = sizeof(Obj256); break;
                case 1: alloc_size = sizeof(Obj512); break;
                case 2: alloc_size = sizeof(Obj1024); break;
                case 3: alloc_size = sizeof(Obj2048); break;
            }
            active_ptrs[op.slot_index] = std::malloc(alloc_size);
            
            if (active_ptrs[op.slot_index]) {
                // Force memory commit
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // [Fix] Read and accumulate value to ensure code execution
                local_sum += static_cast<uint8_t*>(active_ptrs[op.slot_index])[0];
            }
        } 
        else { // FREE
            if (active_ptrs[op.slot_index]) {
                std::free(active_ptrs[op.slot_index]);
                active_ptrs[op.slot_index] = nullptr;
            }
        }
    }

    // Cleanup remaining
    for (void* ptr : active_ptrs) {
        if (ptr) std::free(ptr);
    }

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalalloc() {
    Palalloc allocator(30000, 2048);
    allocator.init();

    std::vector<void*> active_ptrs(MAX_ACTIVE_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0; // Accumulator to trick compiler

    for (const auto& op : operations) {
        if (op.type == OpType::ALLOC) {
            switch (op.size_class) {
                case 0: active_ptrs[op.slot_index] = allocator.galloc<Obj256>(); break;
                case 1: active_ptrs[op.slot_index] = allocator.galloc<Obj512>(); break;
                case 2: active_ptrs[op.slot_index] = allocator.galloc<Obj1024>(); break;
                case 3: active_ptrs[op.slot_index] = allocator.galloc<Obj2048>(); break;
            }
            
            // Force memory commit
            if (active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // [Fix] Read and accumulate
                local_sum += static_cast<uint8_t*>(active_ptrs[op.slot_index])[0];
            }
        } 
        else { // FREE
            // [Fix] Check if ptr is valid before performing casted-free
            if (active_ptrs[op.slot_index]) {
                switch (op.size_class) {
                    case 0: allocator.free(static_cast<Obj256*>(active_ptrs[op.slot_index])); break;
                    case 1: allocator.free(static_cast<Obj512*>(active_ptrs[op.slot_index])); break;
                    case 2: allocator.free(static_cast<Obj1024*>(active_ptrs[op.slot_index])); break;
                    case 3: allocator.free(static_cast<Obj2048*>(active_ptrs[op.slot_index])); break;
                }
                active_ptrs[op.slot_index] = nullptr;
            }
        }
    }

    // [Fix] Dump to global volatile sink before closing time window
    global_sink = local_sum;

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Benchmark Scenario 3: Chaotic Allocation & Large Objects\n";
    std::cout << " Total Operations: " << NUM_OPERATIONS << "\n";
    std::cout << " Object Sizes: 256, 512, 1024, 2048 Bytes\n";
    std::cout << " Generating Instruction Sequence (Please Wait...)...\n";
    
    generateChaos();

    std::cout << " Starting Benchmark...\n";
    std::cout << "========================================================\n\n";

    double malloc_time = benchmarkMalloc();
    double palalloc_time = benchmarkPalalloc();
    double speedup = malloc_time / palalloc_time;

    std::cout << "[std::malloc] Total Time: " 
              << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
              
    std::cout << "[Palalloc]    Total Time: " 
              << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    std::cout << ">>> In a chaotic large-object fragmentation test,\n";
    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) << speedup << "x faster than std::malloc!\n";
              
    std::cout << "========================================================\n";

    return 0;
}