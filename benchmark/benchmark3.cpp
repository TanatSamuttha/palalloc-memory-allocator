#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "paralloc.h"

// โครงสร้างข้อมูลขนาดต่างๆ
struct InputData    { uint64_t buttons; };                                      // 8B
struct Bullet       { float x, y, z, w; };                                      // 16B
struct Player       { char name[16]; int hp; int mp; float speed; };            // 32B
struct Effect       { char data[40]; };                                         // 40B (ลงพูล 64B)
struct Command      { char data[56]; };                                         // 56B (ลงพูล 64B)
struct HeavyPayload { char chatMessage[128]; };                                 // 128B (Fallback)

// โครงสร้างสำหรับจัดการอายุขัย (Time-to-Live)
template <typename T>
struct Entity {
    T* ptr = nullptr;
    int ttl = 0; // จำนวนเฟรมที่เหลืออยู่ก่อนจะโดน Free
};

const int TOTAL_FRAMES = 100'000'000; // จำลองการรันเกม/เซิร์ฟเวอร์ 100,000 เฟรม
inline uint32_t fast_rand(uint32_t& seed) {
    seed = seed * 1664525 + 1013904223;
    return seed;
}

double benchmarkMalloc() {
    uint32_t seed = 42;
    auto start = std::chrono::high_resolution_clock::now();

    // ถังเก็บ Entity ในระบบ
    Entity<Player> activePlayers[20];
    Entity<Bullet> activeBullets[128];
    Entity<Effect> activeEffects[64];

    for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
        // 1. ทุกๆ เฟรม มีการจอง Input (เกิดไวตายไวในเฟรมเดียว)
        for (int i = 0; i < 5; ++i) {
            InputData* in = static_cast<InputData*>(std::malloc(sizeof(InputData)));
            std::free(in);
        }

        // 2. อัปเดตอายุขัย (TTL) ของข้อมูลเดิมที่มีอยู่ (ถ้าหมดอายุขัย ให้ Free ทิ้ง)
        for (auto& p : activePlayers) {
            if (p.ptr) {
                p.ttl--;
                if (p.ttl <= 0) { std::free(p.ptr); p.ptr = nullptr; }
            }
        }
        for (auto& b : activeBullets) {
            if (b.ptr) {
                b.ttl--;
                if (b.ttl <= 0) { std::free(b.ptr); b.ptr = nullptr; }
            }
        }
        for (auto& e : activeEffects) {
            if (e.ptr) {
                e.ttl--;
                if (e.ttl <= 0) { std::free(e.ptr); e.ptr = nullptr; }
            }
        }

        // 3. จำลองสภาพแวดล้อม: เฟรมปกติ VS เฟรมศึกหนัก (Spike Frame)
        bool isSpikeFrame = (frame % 100 == 0); // เกิดการระเบิดใหญ่ทุกๆ 100 เฟรม
        int bulletsToSpawn = isSpikeFrame ? 40 : (fast_rand(seed) % 3);
        int effectsToSpawn = isSpikeFrame ? 20 : (fast_rand(seed) % 2);

        // จองเมมให้กระสุนใหม่
        for (int i = 0; i < bulletsToSpawn; ++i) {
            int slot = fast_rand(seed) % 128;
            if (!activeBullets[slot].ptr) { // ถ้าช่องว่าง ให้จอง
                activeBullets[slot].ptr = static_cast<Bullet*>(std::malloc(sizeof(Bullet)));
                activeBullets[slot].ttl = 10 + (fast_rand(seed) % 20); // อยู่ได้ 10-30 เฟรม
            }
        }

        // จองเมมให้เอฟเฟกต์ใหม่
        for (int i = 0; i < effectsToSpawn; ++i) {
            int slot = fast_rand(seed) % 64;
            if (!activeEffects[slot].ptr) {
                activeEffects[slot].ptr = static_cast<Effect*>(std::malloc(sizeof(Effect)));
                activeEffects[slot].ttl = 5 + (fast_rand(seed) % 10); // เอฟเฟกต์ตายไว อยู่ได้ 5-15 เฟรม
            }
        }

        // สุ่มผู้เล่นเข้า/ออกเกม (แต่นานๆ ที)
        if (fast_rand(seed) % 100 < 5) {
            int slot = fast_rand(seed) % 20;
            if (!activePlayers[slot].ptr) {
                activePlayers[slot].ptr = static_cast<Player*>(std::malloc(sizeof(Player)));
                activePlayers[slot].ttl = 200 + (fast_rand(seed) % 500); // ผู้เล่นอยู่นานมาก
            }
        }

        // มี Command วิ่งเข้ามาประมวลผลเป็นระยะ
        if (fast_rand(seed) % 10 < 3) {
            Command* cmd = static_cast<Command*>(std::malloc(sizeof(Command)));
            std::free(cmd);
        }

        // นานๆ ทีจะมีข้อความแชทขนาดใหญ่ (Oversize)
        if (fast_rand(seed) % 1000 < 5) {
            HeavyPayload* msg = static_cast<HeavyPayload*>(std::malloc(sizeof(HeavyPayload)));
            std::free(msg);
        }
    }

    // ล้างไพ่ตอนจบเกม
    for (auto& p : activePlayers) if (p.ptr) std::free(p.ptr);
    for (auto& b : activeBullets) if (b.ptr) std::free(b.ptr);
    for (auto& e : activeEffects) if (e.ptr) std::free(e.ptr);

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double benchmarkParalloc() {
    uint32_t seed = 42;
    Paralloc pool;
    auto start = std::chrono::high_resolution_clock::now();

    Entity<Player> activePlayers[20];
    Entity<Bullet> activeBullets[128];
    Entity<Effect> activeEffects[64];

    for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
        for (int i = 0; i < 5; ++i) {
            InputData* in = pool.galloc<InputData>();
            pool.free(in);
        }

        for (auto& p : activePlayers) {
            if (p.ptr) {
                p.ttl--;
                if (p.ttl <= 0) { pool.free(p.ptr); p.ptr = nullptr; }
            }
        }
        for (auto& b : activeBullets) {
            if (b.ptr) {
                b.ttl--;
                if (b.ttl <= 0) { pool.free(b.ptr); b.ptr = nullptr; }
            }
        }
        for (auto& e : activeEffects) {
            if (e.ptr) {
                e.ttl--;
                if (e.ttl <= 0) { pool.free(e.ptr); e.ptr = nullptr; }
            }
        }

        bool isSpikeFrame = (frame % 100 == 0);
        int bulletsToSpawn = isSpikeFrame ? 40 : (fast_rand(seed) % 3);
        int effectsToSpawn = isSpikeFrame ? 20 : (fast_rand(seed) % 2);

        for (int i = 0; i < bulletsToSpawn; ++i) {
            int slot = fast_rand(seed) % 128;
            if (!activeBullets[slot].ptr) {
                activeBullets[slot].ptr = pool.galloc<Bullet>();
                activeBullets[slot].ttl = 10 + (fast_rand(seed) % 20);
            }
        }

        for (int i = 0; i < effectsToSpawn; ++i) {
            int slot = fast_rand(seed) % 64;
            if (!activeEffects[slot].ptr) {
                activeEffects[slot].ptr = pool.galloc<Effect>();
                activeEffects[slot].ttl = 5 + (fast_rand(seed) % 10);
            }
        }

        if (fast_rand(seed) % 100 < 5) {
            int slot = fast_rand(seed) % 20;
            if (!activePlayers[slot].ptr) {
                activePlayers[slot].ptr = pool.galloc<Player>();
                activePlayers[slot].ttl = 200 + (fast_rand(seed) % 500);
            }
        }

        if (fast_rand(seed) % 10 < 3) {
            Command* cmd = pool.galloc<Command>();
            pool.free(cmd);
        }

        if (fast_rand(seed) % 1000 < 5) {
            HeavyPayload* msg = pool.galloc<HeavyPayload>();
            pool.free(msg);
        }
    }

    for (auto& p : activePlayers) if (p.ptr) pool.free(p.ptr);
    for (auto& b : activeBullets) if (b.ptr) pool.free(b.ptr);
    for (auto& e : activeEffects) if (e.ptr) pool.free(e.ptr);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    double totalAllocs = pool.parallocUsed + pool.mallocUsed;
    std::cout << "[Paralloc Pool  ]\n";
    std::cout << "  -> Pool Success      : " << pool.parallocUsed << " times (" << (pool.parallocUsed / totalAllocs) * 100.0 << "%)\n";
    std::cout << "  -> Fallback (Malloc) : " << pool.mallocUsed << " times (" << (pool.mallocUsed / totalAllocs) * 100.0 << "%)\n";

    return elapsed;
}

int main() {
    std::cout << "Running Real-World Game Loop Simulator (" << TOTAL_FRAMES << " frames)...\n";
    
    benchmarkMalloc(); 
    
    double tMalloc = benchmarkMalloc();
    std::cout << "[Standard Malloc] Time: " << std::fixed << std::setprecision(2) << tMalloc << " ms\n";

    double tParalloc = benchmarkParalloc();
    std::cout << "Time: " << std::fixed << std::setprecision(2) << tParalloc << " ms\n";
    
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "[SUMMARY] Paralloc is " << (tMalloc / tParalloc) << "x faster than Standard Malloc!\n";
    return 0;
}