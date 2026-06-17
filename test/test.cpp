#include <iostream>
#include <cassert>
#include <vector>
#include <set>
#include "palalloc.h"

// Mock structure representing in-game data (16 Bytes)
struct Transform {
    float x, y, z;
    uint32_t id;
};

void runCorrectionTest() {
    std::cout << "--- Running Correction Test ---\n";

    // 1. Setup: Create a pool of 100 pages (approx 400KB)
    Palalloc allocator(100, 64);
    allocator.init();

    const int TEST_COUNT = 1000;
    std::set<Transform*> unique_ptrs;

    // 2. Uniqueness Test: Ensure all allocated pointers are distinct
    for (int i = 0; i < TEST_COUNT; ++i) {
        Transform* p = allocator.alloc<Transform>();
        assert(p != nullptr && "Allocation failed unexpectedly!");
        
        bool is_new = unique_ptrs.insert(p).second;
        assert(is_new && "Duplicate pointer detected! Pool memory overlap.");
    }
    std::cout << "[OK] Uniqueness test passed.\n";

    // 3. Reset Test
    allocator.reset();
    unique_ptrs.clear();
    for (int i = 0; i < TEST_COUNT; ++i) {
        Transform* p = allocator.alloc<Transform>();
        assert(p != nullptr && "Allocation failed after reset!");
        unique_ptrs.insert(p);
    }
    std::cout << "[OK] Reset test passed.\n";

    // 4. Exhaustion Test: Corrected to handle loop termination
    // We use a large number to ensure we hit the pool limit
    bool got_null = false;
    int max_possible_allocs = 500000; 
    
    for (int i = 0; i < 500000; ++i) {
        Transform* p = allocator.alloc<Transform>();
        
        if (p == nullptr) {
            got_null = true;
            std::cout << "[OK] Pool exhausted at iteration: " << i << "\n";
            break;
        }

        // เช็คสถานะภายในทุกๆ 50,000 ครั้ง
        if (i % 50000 == 0) {
            std::cout << "Iteration " << i 
                      << " | Head: " << allocator.getHead<Transform>() 
                      << " | Virgin: " << allocator.getVirgin<Transform>() << "\n";
        }
    }
    if (!got_null) {
        std::cerr << "[Error] Pool did not return nullptr even after " << max_possible_allocs << " allocations!\n";
    }
    assert(got_null && "Pool failed to return nullptr on exhaustion!");

    std::cout << ">>> All Correction Tests Passed Successfully!\n";
}

int main() {
    try {
        runCorrectionTest();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}