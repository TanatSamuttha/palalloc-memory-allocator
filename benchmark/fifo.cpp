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

struct Obj16  { uint8_t data[16];  }; // Class 0
struct Obj32  { uint8_t data[32];  }; // Class 1
struct Obj64  { uint8_t data[64];  }; // Class 2
struct Obj128 { uint8_t data[128]; }; // Class 3

const int NUM_OPERATIONS = 5000000; 
const int MAX_ACTIVE_OBJECTS = 250000; 

enum class OpType { ALLOC, FREE };

struct Instruction {
    OpType type;
    int slot_index;
    int size_class;
};

std::vector<Instruction> operations;

void generateFIFOChaos() {
    std::mt19937 rng(42); 
    std::uniform_int_distribution<int> dist_action(0, 100);
    
    operations.clear();
    operations.reserve(NUM_OPERATIONS);

    std::vector<int> slot_sizes(MAX_ACTIVE_OBJECTS, -1);

    // ----------------------------------------------------------------
    // Wave 1: Warm-up - Fill the pipeline completely (All Allocations)
    // ----------------------------------------------------------------
    for (int slot = 0; slot < MAX_ACTIVE_OBJECTS; ++slot) {
        int size_roll = dist_action(rng);
        int size_class = 0;
        
        if (size_roll < 85)       size_class = 1; // 85% chance: 32B
        else if (size_roll < 95)  size_class = 0; // 10% chance: 16B
        else if (size_roll < 98)  size_class = 2; // 3% chance: 64B
        else                      size_class = 3; // 2% chance: 128B

        operations.push_back({OpType::ALLOC, slot, size_class});
        slot_sizes[slot] = size_class;
    }

    // ----------------------------------------------------------------
    // Wave 2: FIFO Waves (Batch Free followed by Batch Alloc)
    // Releasing 50,000 objects at once (~1.6 MB) will flush the L1/L2 Cache
    // ----------------------------------------------------------------
    const int BATCH_SIZE = 50000; 
    int current_op_count = MAX_ACTIVE_OBJECTS;

    int free_ptr = 0;  // Head of the FIFO queue (Dequeue/Free side)
    int alloc_ptr = 0; // Tail of the FIFO queue (Enqueue/Alloc side)

    while (current_op_count < NUM_OPERATIONS) {
        
        // 1. FREE the oldest objects in batches (FIFO Free)
        for (int i = 0; i < BATCH_SIZE && current_op_count < NUM_OPERATIONS; ++i) {
            int slot = free_ptr;
            if (slot_sizes[slot] != -1) {
                operations.push_back({OpType::FREE, slot, slot_sizes[slot]});
                slot_sizes[slot] = -1; // Clear the occupied status
                current_op_count++;
            }
            free_ptr = (free_ptr + 1) % MAX_ACTIVE_OBJECTS;
        }

        // 2. ALLOC new objects to replace the freed ones in batches (FIFO Alloc)
        for (int i = 0; i < BATCH_SIZE && current_op_count < NUM_OPERATIONS; ++i) {
            int slot = alloc_ptr;
            int size_roll = dist_action(rng);
            int size_class = 0;
            
            if (size_roll < 85)       size_class = 1;
            else if (size_roll < 95)  size_class = 0;
            else if (size_roll < 98)  size_class = 2;
            else                      size_class = 3;

            operations.push_back({OpType::ALLOC, slot, size_class});
            slot_sizes[slot] = size_class;
            current_op_count++;
            alloc_ptr = (alloc_ptr + 1) % MAX_ACTIVE_OBJECTS;
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
            
            if (active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // Prevent compiler from eliminating this write and allocation lifecycle
                prevent_optimization(active_ptrs[op.slot_index]);
            }
        } 
        else {
            std::free(active_ptrs[op.slot_index]);
            active_ptrs[op.slot_index] = nullptr;
        }
    }

    for (void* ptr : active_ptrs) {
        if (ptr) std::free(ptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

double benchmarkPalalloc() {
    const size_t TIGHT_POOL_PAGES = 5000; 
    const size_t MAX_SIZE = 128;
    
    Palalloc allocator(TIGHT_POOL_PAGES, MAX_SIZE);
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
            
            if(active_ptrs[op.slot_index]) {
                static_cast<uint8_t*>(active_ptrs[op.slot_index])[0] = 0xFF;
                // Prevent compiler from eliminating this write and allocation lifecycle
                prevent_optimization(active_ptrs[op.slot_index]);
            }
        } 
        else {
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
    std::cout << " Benchmark Scenario: The Conveyor Belt (Batch FIFO)\n";
    std::cout << " Defeating LIFO Cache Locality with 50,000-object waves\n";
    std::cout << " Total Operations: " << NUM_OPERATIONS << "\n";
    std::cout << " Generating FIFO Sequence (Please Wait...)...\n";
    
    // Execute the FIFO chaos generator
    generateFIFOChaos();

    std::cout << " Starting Benchmark...\n";
    std::cout << "========================================================\n\n";

    double malloc_time = benchmarkMalloc();
    double palalloc_time = benchmarkPalalloc();
    double speedup = malloc_time / palalloc_time;

    std::cout << "[std::malloc] Total Time: " 
              << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
              
    std::cout << "[Palalloc]    Total Time: " 
              << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    std::cout << ">>> Speedup in Cold Cache (FIFO) scenario:\n";
    std::cout << ">>> Palalloc is " << std::fixed << std::setprecision(2) << speedup << "x faster than std::malloc\n";
              
    std::cout << "========================================================\n";

    return 0;
}