#include <iostream>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdint>

#include "palalloc.h"
#include "palalloc_wrapper.h"

// =========================================================
// Benchmark Configuration
// =========================================================

constexpr int INITIAL_KEYS = 100000;
constexpr int OPERATIONS   = 3000000;
constexpr int RUNS         = 5;

// =========================================================
// Payload
//
// A larger payload is used to avoid benchmarking only the
// hash table logic. Each node occupies meaningful memory,
// making allocator behavior observable.
// =========================================================

struct Payload {
    uint64_t values[8];

    Payload() {
        values[0] = 1;
    }
};

// =========================================================
// Operation Stream
// =========================================================

enum class OpType {
    FIND,
    INSERT_UPDATE,
    ERASE_REPLACE
};

struct Operation {
    OpType type;
    int key;
    Payload value;
};

std::vector<Operation> workload;

// =========================================================
// Prevent dead-code elimination under -O3
// =========================================================

volatile uint64_t global_sink = 0;

// =========================================================
// Generate reproducible workload
// =========================================================

void generateWorkload() {
    std::mt19937 rng(42);

    std::uniform_int_distribution<int> keyDist(
        0,
        INITIAL_KEYS * 20
    );

    std::uniform_int_distribution<int> actionDist(
        0,
        99
    );

    workload.reserve(OPERATIONS);

    for (int i = 0; i < OPERATIONS; ++i) {

        int roll = actionDist(rng);

        Operation op;

        if (roll < 50) {
            op.type = OpType::FIND;
        }
        else if (roll < 80) {
            op.type = OpType::INSERT_UPDATE;
        }
        else {
            op.type = OpType::ERASE_REPLACE;
        }

        op.key = keyDist(rng);

        op.value.values[0] = static_cast<uint64_t>(i);
        op.value.values[1] = static_cast<uint64_t>(op.key);

        workload.push_back(op);
    }
}

// =========================================================
// Standard Allocator Benchmark
// =========================================================

double benchmarkStd() {

    using MapType =
        std::unordered_map<int, Payload>;

    MapType map;

    // Reserve buckets in advance.
    //
    // The purpose of this benchmark is to evaluate
    // allocator performance during node creation
    // and destruction.
    //
    // Rehashing is intentionally minimized because
    // it primarily measures hash-table growth policy.
    map.reserve(INITIAL_KEYS * 2);

    for (int i = 0; i < INITIAL_KEYS; ++i) {
        map.emplace(i, Payload{});
    }

    uint64_t checksum = 0;

    auto start =
        std::chrono::high_resolution_clock::now();

    for (const auto& op : workload) {

        switch (op.type) {

            case OpType::FIND: {
                auto it = map.find(op.key);

                if (it != map.end()) {
                    checksum += it->second.values[0];
                }
                break;
            }

            case OpType::INSERT_UPDATE: {
                map[op.key] = op.value;
                break;
            }

            case OpType::ERASE_REPLACE: {
                map.erase(op.key);

                map[op.key + INITIAL_KEYS * 50] =
                    op.value;
                break;
            }
        }
    }

    for (const auto& [k, v] : map) {
        checksum += k;
        checksum += v.values[0];
    }

    global_sink = checksum;

    auto end =
        std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

// =========================================================
// Palalloc Benchmark
// =========================================================

double benchmarkPalalloc() {

    using PairType =
        std::pair<const int, Payload>;

    Palalloc allocator(
        4096 * 5,
        128
    );

    PalallocWrapper<PairType> wrapper(
        allocator
    );

    using MapType =
        std::unordered_map<
            int,
            Payload,
            std::hash<int>,
            std::equal_to<int>,
            PalallocWrapper<PairType>
        >;

    MapType map(wrapper);

    map.reserve(INITIAL_KEYS * 2);

    for (int i = 0; i < INITIAL_KEYS; ++i) {
        map.emplace(i, Payload{});
    }

    uint64_t checksum = 0;

    auto start =
        std::chrono::high_resolution_clock::now();

    for (const auto& op : workload) {

        switch (op.type) {

            case OpType::FIND: {
                auto it = map.find(op.key);

                if (it != map.end()) {
                    checksum += it->second.values[0];
                }
                break;
            }

            case OpType::INSERT_UPDATE: {
                map[op.key] = op.value;
                break;
            }

            case OpType::ERASE_REPLACE: {
                map.erase(op.key);

                map[op.key + INITIAL_KEYS * 50] =
                    op.value;
                break;
            }
        }
    }

    for (const auto& [k, v] : map) {
        checksum += k;
        checksum += v.values[0];
    }

    global_sink = checksum;

    auto end =
        std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

// =========================================================
// Run Multiple Times
// =========================================================

template<typename Func>
double bestOf(Func benchmark) {

    double best = 1e100;

    for (int i = 0; i < RUNS; ++i) {
        best = std::min(best, benchmark());
    }

    return best;
}

// =========================================================
// Main
// =========================================================

int main() {

    std::cout
        << "========================================================\n";

    std::cout
        << " unordered_map Allocator Benchmark\n";

    std::cout
        << "========================================================\n";

    std::cout
        << " Initial Entries : "
        << INITIAL_KEYS
        << "\n";

    std::cout
        << " Operations      : "
        << OPERATIONS
        << "\n";

    std::cout
        << " Workload Mix    :\n";

    std::cout
        << "   50% lookup\n";

    std::cout
        << "   30% insert/update\n";

    std::cout
        << "   20% erase+replace\n";

    std::cout
        << " Runs            : "
        << RUNS
        << " (best result)\n";

    std::cout
        << "\nGenerating workload...\n";

    generateWorkload();

    std::cout
        << "Starting benchmark...\n\n";

    double stdTime =
        bestOf(benchmarkStd);

    double palallocTime =
        bestOf(benchmarkPalalloc);

    double speedup =
        stdTime / palallocTime;

    std::cout
        << "[std::unordered_map] "
        << std::fixed
        << std::setprecision(2)
        << stdTime
        << " ms\n";

    std::cout
        << "[Palalloc]           "
        << std::fixed
        << std::setprecision(2)
        << palallocTime
        << " ms\n\n";

    std::cout
        << "Speedup: "
        << std::fixed
        << std::setprecision(2)
        << speedup
        << "x\n";

    std::cout
        << "========================================================\n";

    return 0;
}