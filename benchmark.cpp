#include <iostream>
#include <vector>
#include <random>
#include <bitset>

struct Operation
{
    int idx;
    bool code;
};

const int n = 1000000;
const int maxAlive = 200000;
std::vector<int> allocated;
std::vector<int> freeIdx;

std::vector<Operation> operations;
std::vector<void*> ptrs;

int main ()
{
    std::mt19937 gen(12345);
    std::uniform_real_distribution<float> floatDist(0.0f, 1.0f);

    int peakAlive = 0;
    int allocation = 0;
    int freeTimes = 0;

    for (int i = 0; i < n; ++i)
    {
        float rand = floatDist(gen);
        if ((allocated.empty() || rand <= 0.6) && allocated.size() < maxAlive)
        {
            operations.push_back(Operation{0, 0});
            if (!freeIdx.empty())
            {
                allocated.push_back(*(freeIdx.end() - 1));
                freeIdx.pop_back();
            }
            else
            {
                allocated.push_back(allocated.size());
            }
            peakAlive = std::max(peakAlive, (int)allocated.size());
            ++allocation;
        }
        else
        {
            std::uniform_int_distribution intDist(0, (int)(allocated.size() - 1));
            int idx = intDist(gen);
            operations.push_back(Operation{allocated[idx], 1});
            allocated[idx] = *(allocated.end() - 1);
            allocated.pop_back();
            ++freeTimes;
        }
    }

    std::cout << "Allocation: " << allocation << '\n';
    std::cout << "Free:       " << freeTimes << '\n';
    std::cout << "Peak alive: " << peakAlive << '\n';

    return 0;
}