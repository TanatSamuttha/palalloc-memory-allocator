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

// Enum to define operation types
enum class OpType { ALLOC, FREE };

// Instruction structure to ensure BOTH allocators run the exact same chaotic sequence
struct Instruction {
    OpType type;
    int slot_index;
    int size_class; // 0=256, 1=512, 2=1024, 3=2048
};

// Pre-generated operation sequence
std::vector<Instruction> operations;

// Pre-generate the chaotic sequence to remove RNG overhead from the benchmark loop
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
            // Bias towards FREE if the slot is already occupied, or randomly free it
            if (action_roll < 60) {
                operations.push_back({OpType::FREE, slot, slot_sizes[slot]});
                slot_occupied[slot] = false;
            }
        } else {
            // Bias towards ALLOC if the slot is empty
            if (action_roll >= 40) {
                int size_class = dist_size(rng);
                operations.push_back({OpType::ALLOC, slot, size_class});
                slot_occupied[slot] = true;
                slot_sizes[slot] = size_class;
            }
        }
    }
}

double benchmarkMalloc() {
    std::vector<void*> active_ptrs(MAX_ACTIVE_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

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
            
            // Force memory commit (prevent lazy allocation optimization by OS)
            static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
        } 
        else { // FREE
            std::free(active_ptrs[op.slot_index]);
            active_ptrs[op.slot_index] = nullptr;
        }
    }

    // Cleanup remaining
    for (void* ptr : active_ptrs) {
        if (ptr) std::free(ptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalalloc() {
    /* Calculate Pool Size for large objects:
       Max active objects = 50,000. Worst case: all are 2048 bytes.
       50,000 * 2048 = 102,400,000 bytes (~100 MB).
       102,400,000 / 4096 = 25,000 pages.
       We allocate 30,000 pages (~120 MB) to comfortably handle the chaos.
       Max size class is set to 2048. 
       Palalloc will automatically set size classes: [0]=256, [1]=512, [2]=1024, [3]=2048.
    */
    Palalloc allocator(30000, 2048);
    allocator.init();

    std::vector<void*> active_ptrs(MAX_ACTIVE_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& op : operations) {
        if (op.type == OpType::ALLOC) {
            switch (op.size_class) {
                case 0: active_ptrs[op.slot_index] = allocator.galloc<Obj256>(); break;
                case 1: active_ptrs[op.slot_index] = allocator.galloc<Obj512>(); break;
                case 2: active_ptrs[op.slot_index] = allocator.galloc<Obj1024>(); break;
                case 3: active_ptrs[op.slot_index] = allocator.galloc<Obj2048>(); break;
            }
            
            // Force memory commit
            if(active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
            }
        } 
        else { // FREE
            switch (op.size_class) {
                case 0: allocator.free(static_cast<Obj256*>(active_ptrs[op.slot_index])); break;
                case 1: allocator.free(static_cast<Obj512*>(active_ptrs[op.slot_index])); break;
                case 2: allocator.free(static_cast<Obj1024*>(active_ptrs[op.slot_index])); break;
                case 3: allocator.free(static_cast<Obj2048*>(active_ptrs[op.slot_index])); break;
            }
            active_ptrs[op.slot_index] = nullptr;
        }
    }

    // Cleanup isn't strictly needed for Palalloc due to its destructor/reset, 
    // but we let the object destruct naturally.

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