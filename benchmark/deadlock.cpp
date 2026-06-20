#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>
#include <random>
#include <algorithm>

#include "palalloc.h"

struct Class0Obj { uint8_t data[16];  };
struct Class1Obj { uint8_t data[32];  };
struct Class2Obj { uint8_t data[64];  };
struct Class3Obj { uint8_t data[128]; };

constexpr int POOL_PAGES = 500;
constexpr int MAX_SIZE_CLASS = 128;
constexpr int BENCHMARK_ROUNDS = 5;

volatile uint64_t global_sink = 0;

template<typename T>
inline void touchObject(T* ptr)
{
    uint64_t sum = 0;

    for(size_t i = 0; i < sizeof(T); i += 4)
        sum += reinterpret_cast<uint8_t*>(ptr)[i];

    global_sink ^= sum;
    global_sink ^= reinterpret_cast<uintptr_t>(ptr);
}

double benchmarkPalallocFragmentation()
{
    Palalloc poolA(POOL_PAGES, MAX_SIZE_CLASS);
    Palalloc poolB(POOL_PAGES, MAX_SIZE_CLASS);

    poolA.init();
    poolB.init();

    Palalloc* active = &poolA;
    Palalloc* standby = &poolB;

    std::mt19937 rng(12345);

    size_t allocFailCount = 0;
    size_t migrationCount = 0;

    auto start =
        std::chrono::high_resolution_clock::now();

    for(int round = 0; round < BENCHMARK_ROUNDS; ++round)
    {
        std::vector<Class0Obj*> class0;
        std::vector<Class1Obj*> class1;
        std::vector<Class2Obj*> class2;
        std::vector<Class3Obj*> class3;

        // =====================================================
        // Phase 1
        // Fill Class0 until allocator refuses more blocks
        // =====================================================

        while(true)
        {
            Class0Obj* ptr =
                active->alloc<Class0Obj>();

            if(ptr == nullptr)
                break;

            ptr->data[0] = 1;
            class0.push_back(ptr);
        }

        // =====================================================
        // Phase 2
        // Checkerboard fragmentation
        // =====================================================

        for(size_t i = 0; i < class0.size(); i += 2)
        {
            active->free(class0[i]);
            class0[i] = nullptr;
        }

        // =====================================================
        // Phase 3
        // Force combine() pressure
        // =====================================================

        while(true)
        {
            Class1Obj* ptr =
                active->alloc<Class1Obj>();

            if(ptr == nullptr)
            {
                allocFailCount++;
                break;
            }

            ptr->data[0] = 2;
            class1.push_back(ptr);
        }

        // =====================================================
        // Phase 4
        // Random free of survivors
        // =====================================================

        for(size_t i = 1; i < class0.size(); i += 2)
        {
            if((rng() & 3) == 0)
            {
                active->free(class0[i]);
                class0[i] = nullptr;
            }
        }

        // =====================================================
        // Phase 5
        // Force larger combine chain
        // =====================================================

        while(true)
        {
            Class2Obj* ptr =
                active->alloc<Class2Obj>();

            if(ptr == nullptr)
            {
                allocFailCount++;
                break;
            }

            ptr->data[0] = 3;
            class2.push_back(ptr);
        }

        // =====================================================
        // Phase 6
        // More random holes
        // =====================================================

        for(size_t i = 0; i < class1.size(); ++i)
        {
            if((rng() & 3) == 0)
            {
                active->free(class1[i]);
                class1[i] = nullptr;
            }
        }

        // =====================================================
        // Phase 7
        // Request largest blocks
        // =====================================================

        while(true)
        {
            Class3Obj* ptr =
                active->alloc<Class3Obj>();

            if(ptr == nullptr)
            {
                allocFailCount++;
                break;
            }

            ptr->data[0] = 4;
            class3.push_back(ptr);
        }

        // =====================================================
        // Phase 8
        // Migration to fresh pool
        // =====================================================

        std::vector<Class0Obj*> migrated;

        for(Class0Obj* oldPtr : class0)
        {
            if(oldPtr == nullptr)
                continue;

            Class0Obj* newPtr =
                standby->galloc<Class0Obj>();

            if(newPtr == nullptr)
                continue;

            std::memcpy(
                newPtr,
                oldPtr,
                sizeof(Class0Obj));

            touchObject(newPtr);

            migrated.push_back(newPtr);
            migrationCount++;
        }

        // =====================================================
        // Destroy fragmented allocator instantly
        // =====================================================

        active->hardReset();

        std::swap(active, standby);
    }

    auto end =
        std::chrono::high_resolution_clock::now();

    double elapsed =
        std::chrono::duration<double, std::milli>(
            end - start
        ).count();

    std::cout
        << "Allocation failures : "
        << allocFailCount
        << '\n';

    std::cout
        << "Migration count     : "
        << migrationCount
        << '\n';

    return elapsed;
}

int main()
{
    std::cout
        << "========================================\n"
        << " PALALLOC FRAGMENTATION STRESS TEST\n"
        << "========================================\n\n";

    double t =
        benchmarkPalallocFragmentation();

    std::cout
        << "\nExecution time : "
        << std::fixed
        << std::setprecision(2)
        << t
        << " ms\n";

    std::cout
        << "Global sink    : "
        << global_sink
        << '\n';

    return 0;
}