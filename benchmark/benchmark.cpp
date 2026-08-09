#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <cstdlib>
#include "palalloc.h"

#if defined(_WIN32)
    #include <malloc.h>
#elif defined(__linux__)
    #include <malloc.h>
#elif defined(__APPLE__)
    #include <malloc/malloc.h>
#endif

// ---------------------------------------------------------
// Object types
// ---------------------------------------------------------
struct Obj256  { uint8_t data[256];  };
struct Obj512  { uint8_t data[512];  };
struct Obj1024 { uint8_t data[1024]; };
struct Obj2048 { uint8_t data[2048]; };

const int NUM_OPERATIONS = 2000000;
const int MAX_ACTIVE_OBJECTS = 50000;

enum class OpType { ALLOC, FREE };

struct Instruction {
    OpType type;
    int slot_index;
    int size_class; // 0=256, 1=512, 2=1024, 3=2048
};

std::vector<Instruction> operations;

// ---------------------------------------------------------
// Statistics
// ---------------------------------------------------------

// Peak total live payload
size_t peak_memory = 0;

// Total number of allocations generated for each size
size_t allocation_count[4] = {0, 0, 0, 0};

// Total number of frees generated for each size
size_t free_count[4] = {0, 0, 0, 0};

// Current number of live objects for each size
size_t current_count[4] = {0, 0, 0, 0};

// Number of live objects for each size at peak memory
size_t peak_count[4] = {0, 0, 0, 0};

// ---------------------------------------------------------
// Return object size from size class
// ---------------------------------------------------------
size_t getObjectSize(int size_class)
{
    switch (size_class) {
        case 0: return sizeof(Obj256);
        case 1: return sizeof(Obj512);
        case 2: return sizeof(Obj1024);
        case 3: return sizeof(Obj2048);
        default: return 0;
    }
}

// ---------------------------------------------------------
// Generate chaotic allocation/free sequence
// ---------------------------------------------------------
void generateChaos()
{
    std::mt19937 rng(42);

    std::uniform_int_distribution<int> dist_action(0, 100);
    std::uniform_int_distribution<int> dist_size(0, 3);
    std::uniform_int_distribution<int> dist_slot(
        0,
        MAX_ACTIVE_OBJECTS - 1
    );

    std::vector<bool> slot_occupied(
        MAX_ACTIVE_OBJECTS,
        false
    );

    std::vector<int> slot_sizes(
        MAX_ACTIVE_OBJECTS,
        -1
    );

    operations.reserve(NUM_OPERATIONS);

    size_t current_memory = 0;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {

        int slot = dist_slot(rng);
        int action_roll = dist_action(rng);

        // -------------------------------------------------
        // Occupied slot -> possibly FREE
        // -------------------------------------------------
        if (slot_occupied[slot]) {

            if (action_roll < 60) {

                int size_class = slot_sizes[slot];
                size_t object_size =
                    getObjectSize(size_class);

                operations.push_back({
                    OpType::FREE,
                    slot,
                    size_class
                });

                current_memory -= object_size;
                current_count[size_class]--;

                free_count[size_class]++;

                slot_occupied[slot] = false;
                slot_sizes[slot] = -1;
            }
        }

        // -------------------------------------------------
        // Empty slot -> possibly ALLOC
        // -------------------------------------------------
        else {

            if (action_roll >= 40) {

                int size_class = dist_size(rng);
                size_t object_size =
                    getObjectSize(size_class);

                operations.push_back({
                    OpType::ALLOC,
                    slot,
                    size_class
                });

                slot_occupied[slot] = true;
                slot_sizes[slot] = size_class;

                current_memory += object_size;
                current_count[size_class]++;

                allocation_count[size_class]++;

                // -------------------------------------------------
                // Update peak
                // -------------------------------------------------
                if (current_memory > peak_memory) {

                    peak_memory = current_memory;

                    for (int j = 0; j < 4; ++j) {
                        peak_count[j] = current_count[j];
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------
// Global volatile sink
// ---------------------------------------------------------
volatile uint8_t global_sink = 0;

// ---------------------------------------------------------
// Benchmark std::malloc
// ---------------------------------------------------------
double benchmarkMalloc()
{
    std::vector<void*> active_ptrs(
        MAX_ACTIVE_OBJECTS,
        nullptr
    );

    auto start_time =
        std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0;

    for (const auto& op : operations) {

        if (op.type == OpType::ALLOC) {

            size_t alloc_size =
                getObjectSize(op.size_class);

            active_ptrs[op.slot_index] =
                std::malloc(alloc_size);

            if (active_ptrs[op.slot_index]) {

                static_cast<uint8_t*>(
                    active_ptrs[op.slot_index]
                )[0] = 0xFF;

                local_sum += static_cast<uint8_t*>(
                    active_ptrs[op.slot_index]
                )[0];
            }
        }
        else {

            if (active_ptrs[op.slot_index]) {

                std::free(
                    active_ptrs[op.slot_index]
                );

                active_ptrs[op.slot_index] = nullptr;
            }
        }
    }

    for (void* ptr : active_ptrs) {
        if (ptr) {
            std::free(ptr);
        }
    }

    global_sink = local_sum;

    auto end_time =
        std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed =
        end_time - start_time;

    return elapsed.count();
}

// ---------------------------------------------------------
// Benchmark Palalloc
// ---------------------------------------------------------
double benchmarkPalalloc()
{
    // maxSize = 2048
    //
    // Palalloc size classes:
    //
    // 2048 >> 3 = 256
    // 2048 >> 2 = 512
    // 2048 >> 1 = 1024
    // 2048      = 2048
    //
    Palalloc allocator(10000, 2048);

    allocator.init();

    std::vector<void*> active_ptrs(
        MAX_ACTIVE_OBJECTS,
        nullptr
    );

    auto start_time =
        std::chrono::high_resolution_clock::now();

    uint8_t local_sum = 0;

    for (const auto& op : operations) {

        if (op.type == OpType::ALLOC) {

            switch (op.size_class) {

                case 0:
                    active_ptrs[op.slot_index] =
                        allocator.galloc<Obj256>();
                    break;

                case 1:
                    active_ptrs[op.slot_index] =
                        allocator.galloc<Obj512>();
                    break;

                case 2:
                    active_ptrs[op.slot_index] =
                        allocator.galloc<Obj1024>();
                    break;

                case 3:
                    active_ptrs[op.slot_index] =
                        allocator.galloc<Obj2048>();
                    break;
            }

            if (active_ptrs[op.slot_index]) {

                static_cast<uint8_t*>(
                    active_ptrs[op.slot_index]
                )[0] = 0xFF;

                local_sum += static_cast<uint8_t*>(
                    active_ptrs[op.slot_index]
                )[0];
            }
        }
        else {

            if (active_ptrs[op.slot_index]) {

                switch (op.size_class) {

                    case 0:
                        allocator.free(
                            static_cast<Obj256*>(
                                active_ptrs[op.slot_index]
                            )
                        );
                        break;

                    case 1:
                        allocator.free(
                            static_cast<Obj512*>(
                                active_ptrs[op.slot_index]
                            )
                        );
                        break;

                    case 2:
                        allocator.free(
                            static_cast<Obj1024*>(
                                active_ptrs[op.slot_index]
                            )
                        );
                        break;

                    case 3:
                        allocator.free(
                            static_cast<Obj2048*>(
                                active_ptrs[op.slot_index]
                            )
                        );
                        break;
                }

                active_ptrs[op.slot_index] = nullptr;
            }
        }
    }

    global_sink = local_sum;

    auto end_time =
        std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed =
        end_time - start_time;

    return elapsed.count();
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------
int main()
{
    std::cout
        << "========================================================\n";

    std::cout
        << " Benchmark Scenario 3: Chaotic Allocation & Large Objects\n";

    std::cout
        << " Total Operations: "
        << NUM_OPERATIONS
        << "\n";

    std::cout
        << " Object Sizes: 256, 512, 1024, 2048 Bytes\n";

    std::cout
        << " Generating Instruction Sequence (Please Wait...)...\n";

    generateChaos();

    // =====================================================
    // Workload statistics
    // =====================================================

    std::cout
        << "\n========================================================\n";

    std::cout
        << " Workload Statistics\n";

    std::cout
        << "========================================================\n";

    const char* names[4] = {
        "256",
        "512",
        "1024",
        "2048"
    };

    for (int i = 0; i < 4; ++i) {

        size_t payload =
            allocation_count[i] *
            getObjectSize(i);

        std::cout
            << "\n[" << names[i] << " bytes]\n";

        std::cout
            << "  Allocations : "
            << allocation_count[i]
            << "\n";

        std::cout
            << "  Frees       : "
            << free_count[i]
            << "\n";

        std::cout
            << "  Total data allocated : "
            << payload
            << " bytes ("
            << std::fixed
            << std::setprecision(2)
            << static_cast<double>(payload)
                / (1024.0 * 1024.0)
            << " MiB)\n";

        std::cout
            << "  Peak live slots      : "
            << peak_count[i]
            << "\n";

        std::cout
            << "  Peak live data       : "
            << peak_count[i] *
               getObjectSize(i)
            << " bytes\n";
    }

    // =====================================================
    // Total statistics
    // =====================================================

    size_t total_allocations = 0;

    for (int i = 0; i < 4; ++i) {
        total_allocations += allocation_count[i];
    }

    std::cout
        << "\n--------------------------------------------------------\n";

    std::cout
        << "Total allocation operations: "
        << total_allocations
        << "\n";

    std::cout
        << "Total instruction count: "
        << operations.size()
        << "\n";

    std::cout
        << "Peak Live Memory: "
        << peak_memory
        << " bytes\n";

    std::cout
        << "Peak Live Memory: "
        << std::fixed
        << std::setprecision(2)
        << static_cast<double>(peak_memory)
            / (1024.0 * 1024.0)
        << " MiB\n";

    std::cout
        << "Minimum Pages Required (payload only): "
        << ((peak_memory + 4095) / 4096)
        << " pages\n";

    // =====================================================
    // Palalloc size classes
    // =====================================================

    const size_t maxSize = 2048;

    const size_t slot256  = maxSize >> 3;
    const size_t slot512  = maxSize >> 2;
    const size_t slot1024 = maxSize >> 1;
    const size_t slot2048 = maxSize;

    std::cout
        << "\n========================================================\n";

    std::cout
        << " Palalloc Slot Configuration\n";

    std::cout
        << "========================================================\n";

    std::cout
        << "maxSize = "
        << maxSize
        << " bytes\n\n";

    std::cout
        << "Size Class | Slot Size | Slots in 6000 pages\n";
    std::cout
        << "-----------|-----------|--------------------\n";

    std::cout
        << std::setw(10) << "256 B"
        << " | "
        << std::setw(9) << slot256
        << " | "
        << std::setw(18)
        << (6000ULL * 4096 / 2 / slot256)
        << "\n";

    std::cout
        << std::setw(10) << "512 B"
        << " | "
        << std::setw(9) << slot512
        << " | "
        << std::setw(18)
        << (6000ULL * 4096 / 4 / slot512)
        << "\n";

    std::cout
        << std::setw(10) << "1024 B"
        << " | "
        << std::setw(9) << slot1024
        << " | "
        << std::setw(18)
        << (6000ULL * 4096 / 8 / slot1024)
        << "\n";

    std::cout
        << std::setw(10) << "2048 B"
        << " | "
        << std::setw(9) << slot2048
        << " | "
        << std::setw(18)
        << (6000ULL * 4096 / 8 / slot2048)
        << "\n";

    std::cout
        << "\n";

    // =====================================================
    // Benchmark
    // =====================================================

    std::cout
        << "Starting Benchmark...\n";

    std::cout
        << "========================================================\n\n";

    double malloc_time = benchmarkMalloc();
    double palalloc_time = benchmarkPalalloc();

    double speedup =
        malloc_time / palalloc_time;

    std::cout
        << "[std::malloc] Total Time: "
        << std::fixed
        << std::setprecision(2)
        << malloc_time
        << " ms\n";

    std::cout
        << "[Palalloc]    Total Time: "
        << std::fixed
        << std::setprecision(2)
        << palalloc_time
        << " ms\n\n";

    std::cout
        << ">>> In a chaotic large-object fragmentation test,\n";

    std::cout
        << ">>> Palalloc is "
        << std::fixed
        << std::setprecision(2)
        << speedup
        << "x faster than std::malloc!\n";

    std::cout
        << "========================================================\n";

    return 0;
}