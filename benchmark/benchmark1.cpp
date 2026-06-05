#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "paralloc.h"

struct InputData    { uint64_t buttons; };                                     
struct Bullet       { float x, y, z, w; };                                     
struct Player       { char name[16]; int hp; int mp; float speed; };           
struct HeavyPayload { char chatMessage[128]; };                                

const int ITERATIONS = 10'000'000; 

inline uint32_t fast_rand(uint32_t& seed) {
    seed = seed * 1664525 + 1013904223;
    return seed;
}

double benchmarkMalloc() {
    uint32_t seed = 42;
    auto start = std::chrono::high_resolution_clock::now();

    // ใช้ Array ขนาดคงที่ จองค้างไว้บางส่วน
    Player* activePlayers[16] = {nullptr};
    Bullet* activeBullets[64] = {nullptr};

    for (int i = 0; i < ITERATIONS; ++i) {
        // 1. จองและลบข้อมูลสั้นทันที
        InputData* in1 = static_cast<InputData*>(std::malloc(sizeof(InputData)));
        InputData* in2 = static_cast<InputData*>(std::malloc(sizeof(InputData)));
        std::free(in1); std::free(in2);

        // 2. สุ่มจัดการ Player (จองทับตำแหน่งสุ่ม / คืนค่าชิ้นเก่าตัวที่โดนทับ)
        uint32_t pIdx = fast_rand(seed) % 16;
        if (activePlayers[pIdx] && (fast_rand(seed) % 100 < 20)) { // 20% chance to free
            std::free(activePlayers[pIdx]);
            activePlayers[pIdx] = nullptr;
        } else if (!activePlayers[pIdx]) {
            activePlayers[pIdx] = static_cast<Player*>(std::malloc(sizeof(Player)));
        }

        // 3. สุ่มจัดการ Bullet
        uint32_t bIdx = fast_rand(seed) % 64;
        if (activeBullets[bIdx] && (fast_rand(seed) % 100 < 40)) { // 40% chance to free
            std::free(activeBullets[bIdx]);
            activeBullets[bIdx] = nullptr;
        } else if (!activeBullets[bIdx]) {
            activeBullets[bIdx] = static_cast<Bullet*>(std::malloc(sizeof(Bullet)));
        }

        // 4. สุ่มเกิดข้อมูลใหญ่ล้นระบบ (1% chance)
        if (fast_rand(seed) % 100 < 1) {
            HeavyPayload* msg = static_cast<HeavyPayload*>(std::malloc(sizeof(HeavyPayload)));
            std::free(msg);
        }
    }

    for (int i = 0; i < 16; ++i) if (activePlayers[i]) std::free(activePlayers[i]);
    for (int i = 0; i < 64; ++i) if (activeBullets[i]) std::free(activeBullets[i]);

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double benchmarkParalloc() {
    uint32_t seed = 42;
    Paralloc pool;
    auto start = std::chrono::high_resolution_clock::now();

    Player* activePlayers[16] = {nullptr};
    Bullet* activeBullets[64] = {nullptr};

    for (int i = 0; i < ITERATIONS; ++i) {
        InputData* in1 = pool.galloc<InputData>();
        InputData* in2 = pool.galloc<InputData>();
        pool.free(in1); pool.free(in2);

        uint32_t pIdx = fast_rand(seed) % 16;
        if (activePlayers[pIdx] && (fast_rand(seed) % 100 < 20)) {
            pool.free(activePlayers[pIdx]);
            activePlayers[pIdx] = nullptr;
        } else if (!activePlayers[pIdx]) {
            activePlayers[pIdx] = pool.galloc<Player>();
        }

        uint32_t bIdx = fast_rand(seed) % 64;
        if (activeBullets[bIdx] && (fast_rand(seed) % 100 < 40)) {
            pool.free(activeBullets[bIdx]);
            activeBullets[bIdx] = nullptr;
        } else if (!activeBullets[bIdx]) {
            activeBullets[bIdx] = pool.galloc<Bullet>();
        }

        if (fast_rand(seed) % 100 < 1) {
            HeavyPayload* msg = pool.galloc<HeavyPayload>();
            pool.free(msg);
        }
    }

    for (int i = 0; i < 16; ++i) if (activePlayers[i]) pool.free(activePlayers[i]);
    for (int i = 0; i < 64; ++i) if (activeBullets[i]) pool.free(activeBullets[i]);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    double totalAllocs = pool.parallocUsed + pool.mallocUsed;
    std::cout << "[Paralloc Pool  ]\n";
    std::cout << "  -> Pool Success      : " << pool.parallocUsed << " times (" << (pool.parallocUsed / totalAllocs) * 100.0 << "%)\n";
    std::cout << "  -> Fallback (Malloc) : " << pool.mallocUsed << " times (" << (pool.mallocUsed / totalAllocs) * 100.0 << "%)\n";

    return elapsed;
}

int main() {
    std::cout << "Running Clean $O(1)$ Benchmark (" << ITERATIONS << " iterations)...\n";
    
    // อุ่นเครื่อง CPU 
    benchmarkMalloc(); 
    
    double tMalloc = benchmarkMalloc();
    std::cout << "[Standard Malloc] Time: " << std::fixed << std::setprecision(2) << tMalloc << " ms\n";

    double tParalloc = benchmarkParalloc();
    std::cout << "Time: " << std::fixed << std::setprecision(2) << tParalloc << " ms\n";
    
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[SUMMARY] Paralloc is " << (tMalloc / tParalloc) << "x faster than Standard Malloc!\n";
    return 0;
}