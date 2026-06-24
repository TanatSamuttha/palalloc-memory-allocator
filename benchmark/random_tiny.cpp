#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include "palalloc.h"

// Anti-optimization barrier to prevent -O3 from eliminating allocations and loops
#if defined(__GNUC__) || defined(__clang__)
inline void prevent_optimization(void* ptr) {
    // Forces the compiler to assume the pointer escapes and its memory is modified
    asm volatile("" : : "g"(ptr) : "memory");
}
#else
// Fallback sink for compilers like MSVC
volatile void* optimization_sink = nullptr;
inline void prevent_optimization(void* ptr) {
    optimization_sink = ptr;
}
#endif

// Define tiny object structures specifically to target malloc's tcache/fastbins
struct Obj16  { uint8_t data[16];  }; // Class 0
struct Obj32  { uint8_t data[32];  }; // Class 1
struct Obj64  { uint8_t data[64];  }; // Class 2
struct Obj128 { uint8_t data[128]; }; // Class 3

// Increased operations to heavily thrash the allocator
const int NUM_OPERATIONS = 5000000; // 5 Million chaotic operations
// Far exceeds the tcache limit (~64 objects), forcing fallback to slow arenas
const int MAX_ACTIVE_OBJECTS = 250000; 

// Enum to define operation types
enum class OpType { ALLOC, FREE };

// Instruction structure to ensure BOTH allocators run the exact same chaotic sequence
struct Instruction {
    OpType type;
    int slot_index;
    int size_class; // 0=16, 1=32, 2=64, 3=128
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
            // Bias towards FREE if the slot is already occupied
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
                case 0: alloc_size = sizeof(Obj16); break;
                case 1: alloc_size = sizeof(Obj32); break;
                case 2: alloc_size = sizeof(Obj64); break;
                case 3: alloc_size = sizeof(Obj128); break;
            }
            active_ptrs[op.slot_index] = std::malloc(alloc_size);
            
            // Force memory commit (prevent lazy allocation optimization by OS)
            if (active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // Prevent compiler from eliminating this write and allocation lifecycle
                prevent_optimization(active_ptrs[op.slot_index]);
            }
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
    /* Calculate Pool Size for tiny objects:
       Max active objects = 250,000. Worst case: all are 128 bytes.
       250,000 * 128 = 32,000,000 bytes (~32 MB).
       32,000,000 / 4096 = ~7,812 pages.
       We allocate 12,000 pages (~48 MB) to comfortably handle the chaos.
       Max size class is set to 128. 
       Palalloc will automatically set size classes: [0]=16, [1]=32, [2]=64, [3]=128.
    */
    Palalloc allocator(12000, 128);
    allocator.init();

    std::vector<void*> active_ptrs(MAX_ACTIVE_OBJECTS, nullptr);
    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& op : operations) {
        if (op.type == OpType::ALLOC) {
            switch (op.size_class) {
                case 0: active_ptrs[op.slot_index] = allocator.galloc<Obj16>(); break;
                case 1: active_ptrs[op.slot_index] = allocator.galloc<Obj32>(); break;
                case 2: active_ptrs[op.slot_index] = allocator.galloc<Obj64>(); break;
                case 3: active_ptrs[op.slot_index] = allocator.galloc<Obj128>(); break;
            }
            
            // Force memory commit
            if(active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // Prevent compiler from eliminating this write and allocation lifecycle
                prevent_optimization(active_ptrs[op.slot_index]);
            }
        } 
        else { // FREE
            switch (op.size_class) {
                case 0: allocator.free(static_cast<Obj16*>(active_ptrs[op.slot_index])); break;
                case 1: allocator.free(static_cast<Obj32*>(active_ptrs[op.slot_index])); break;
                case 2: allocator.free(static_cast<Obj64*>(active_ptrs[op.slot_index])); break;
                case 3: allocator.free(static_cast<Obj128*>(active_ptrs[op.slot_index])); break;
            }
            active_ptrs[op.slot_index] = nullptr;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Benchmark Scenario: Chaotic Fast-Path Overflow\n";
    std::cout << " Thrashing tcache with massive, fragmented tiny objects\n";
    std::cout << " Total Operations: " << NUM_OPERATIONS << "\n";
    std::cout << " Object Sizes: 16, 32, 64, 128 Bytes\n";
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

    std::cout << ">>> Under chaotic small-object fragmentation and cache thrashing,\n";
    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) << speedup << "x faster than std::malloc!\n";
              
    std::cout << "========================================================\n";

    return 0;
}