#include <iostream>
#include <vector>
#include <random>
#include <utility>
#include <chrono>

#include "palalloc.h"

struct Operation
{
    int idx;
    int size;
    bool code;
};

const int n = 1000000;
const int maxAlive = 200000;

int peakAlive = 0;
int allocation = 0;
int allocateSize[4] = {0, 0, 0, 0};
int freeTimes = 0;

std::vector<std::pair<int, int>> allocated; // index, size
std::vector<int> freeIdx;
std::vector<Operation> operations;
std::vector<void*> ptrs(maxAlive);

std::mt19937 gen(12345);

void generate ()
{
    std::uniform_real_distribution<float> floatDist(0.0f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        float rand = floatDist(gen);
        if ((allocated.empty() || rand <= 0.6) && allocated.size() < maxAlive)
        {
            std::uniform_int_distribution intDist(0, 3);
            int randSize = intDist(gen);
            int size;
            switch (randSize)
            {
                case 0:
                    size = 8;
                    break;
                case 1:
                    size = 16;
                    break;
                case 2:
                    size = 32;
                    break;
                default:
                    size = 64;
                    break;
            }
            
            if (!freeIdx.empty())
            {
                operations.push_back(Operation{*(freeIdx.end() - 1), size, 0});
                allocated.push_back(std::make_pair(*(freeIdx.end() - 1), size));
                freeIdx.pop_back();
            }
            else
            {
                operations.push_back(Operation{(int)allocated.size(), size, 0});
                allocated.push_back(std::make_pair(allocated.size(), size));
            }
            peakAlive = std::max(peakAlive, (int)allocated.size());
            ++allocateSize[randSize];
            ++allocation;
        }
        else
        {
            std::uniform_int_distribution intDist(0, (int)(allocated.size() - 1));
            int idx = intDist(gen);
            operations.push_back(Operation{allocated[idx].first, allocated[idx].second, 1});
            freeIdx.push_back(allocated[idx].first);
            allocated[idx] = *(allocated.end() - 1);
            allocated.pop_back();
            ++freeTimes;
        }
    }
}

double mallocBenchmark ()
{
    auto start = std::chrono::steady_clock::now();

    for (Operation op : operations)
    {
        if (op.code == 0)
        {
            ptrs[op.idx] = std::malloc(op.size);
            asm volatile("" : : "g"(ptrs[op.idx]) : "memory");
        }
        else
        {
            std::free(ptrs[op.idx]);
        }
    }

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration<double>(end - start).count();
}

double palallocBenchmark ()
{
    auto start = std::chrono::steady_clock::now();

    Palalloc pool = pal_create();
    pal_init(&pool);

    for (Operation op : operations)
    {
        if (op.code == 0)
        {
            ptrs[op.idx] = pal_alloc(&pool, op.size);
            asm volatile("" : : "g"(ptrs[op.idx]) : "memory");
        }
        else
        {
            pal_free(&pool, ptrs[op.idx], op.size);
        }
    }

    pal_destroy(&pool);

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration<double>(end - start).count();
}

int main ()
{
    generate();
    double palallocTime = palallocBenchmark();
    double mallocTime = mallocBenchmark();

    std::cout << "Allocation:        " << allocation << " objects\n";
    std::cout << "Free:              " << freeTimes << " objects\n";
    std::cout << "Peak alive:        " << peakAlive << " objects\n";
    std::cout << "Total allocate 8:  " << allocateSize[0] << " objects\n";
    std::cout << "Total allocate 16: " << allocateSize[1] << " objects\n";
    std::cout << "Total allocate 32: " << allocateSize[2] << " objects\n";
    std::cout << "Total allocate 64: " << allocateSize[3] << " objects\n";
    std::cout << "Palalloc time:     " << palallocTime * 1000 << " ms\n";
    std::cout << "Malloc time:       " << mallocTime * 1000 << " ms\n";
    std::cout << "Comparison:        " << mallocTime / palallocTime << "x\n";

    return 0;
}