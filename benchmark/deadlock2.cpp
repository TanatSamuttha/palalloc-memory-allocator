#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include "palalloc.h"

struct BigClass0 { uint8_t data[1024];  };  // 1 KB
struct BigClass1 { uint8_t data[2048];  };  // 2 KB
struct BigClass2 { uint8_t data[4096];  };  // 4 KB
struct BigClass3 { uint8_t data[8192];  };  // 8 KB

const int LARGE_POOL_PAGES = 60000;         // ~245 MB Pool
const int LARGE_MAX_SIZE = 8192;            
const int BENCHMARK_LOOPS = 50;

// ตัวรับค่าระดับ Global volatile เพื่อบังคับให้คอมไพเลอร์ต้องเขียนค่าลงแรมจริง ห้ามยุบทิ้ง
volatile uint8_t anti_o3_sink = 0;

double benchmarkLargeMallocAntiO3() {
    auto start_time = std::chrono::high_resolution_clock::now();
    uint8_t local_sum = 0;

    for (int round = 0; round < BENCHMARK_LOOPS; ++round) {
        std::vector<void*> ptrs;

        // 1. จองและกระจายข้อมูลขนาดใหญ่
        for (int i = 0; i < 15000; ++i) {
            void* ptr = std::malloc(sizeof(BigClass0));
            if (ptr) {
                std::memset(ptr, 0x11, sizeof(BigClass0)); // แก้ไข: เขียนเต็มขนาดวัตถุ
                ptrs.push_back(ptr);
            }
        }

        // 2. ทำลาย Cache Locality โดยการคืนพื้นที่แบบสลับฟันปลา
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            std::free(ptrs[i]);
            ptrs[i] = nullptr;
        }

        // 3. ช่วงทดสอบ: จองวัตถุขนาดใหญ่ลงในช่องว่างแบบไม่คืนทันที
        std::vector<void*> test_ptrs;
        for (int i = 0; i < 4000; ++i) {
            void* ptr = std::malloc(sizeof(BigClass2));
            if (ptr) {
                std::memset(ptr, 0x22, sizeof(BigClass2)); // แก้ไข: เขียนเต็มขนาดวัตถุ
                // อ่านข้อมูลจากส่วนท้ายของบล็อกเพื่อบังคับให้ CPU ต้องสแกนผ่านแนวหน่วยความจำจริง
                local_sum += static_cast<uint8_t*>(ptr)[sizeof(BigClass2) - 1]; 
                test_ptrs.push_back(ptr);
            }
        }

        // เคลียร์ทั้งหมดเมื่อจบโค้ดเฟสทดสอบ
        for (void* ptr : test_ptrs) std::free(ptr);
        for (void* ptr : ptrs) { if (ptr) std::free(ptr); }
    }

    anti_o3_sink = local_sum;
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end_time - start_time).count();
}

double benchmarkLargePalallocAntiO3() {
    Palalloc allocator(LARGE_POOL_PAGES, LARGE_MAX_SIZE);
    allocator.init();

    auto start_time = std::chrono::high_resolution_clock::now();
    uint8_t local_sum = 0;

    for (int round = 0; round < BENCHMARK_LOOPS; ++round) {
        std::vector<BigClass0*> ptrs;

        // 1. จองวัตถุขนาด 1 KB จนเต็มโซนแรก
        for (int i = 0; i < 15000; ++i) {
            BigClass0* ptr = allocator.alloc<BigClass0>();
            if (ptr) {
                std::memset(ptr, 0x11, sizeof(BigClass0)); // แก้ไข: เขียนเต็มขนาดวัตถุ
                ptrs.push_back(ptr);
            }
        }

        // 2. ทำลายแนวหน่วยความจำต่อเนื่อง (Fragmentation)
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            allocator.free(ptrs[i]);
            ptrs[i] = nullptr;
        }

        // 3. ช่วงทดสอบ: บังคับให้ Palalloc ค้นหาพื้นที่และแตกบล็อกลึกๆ บนแรมจริง 
        // โดยห้ามหยิบบล็อกเดิมที่เพิ่ง free ไปมาเวียนเทียนใช้ใหม่
        std::vector<BigClass2*> test_ptrs;
        for (int i = 0; i < 4000; ++i) {
            BigClass2* ptr = allocator.alloc<BigClass2>();
            if (ptr) {
                std::memset(ptr, 0x22, sizeof(BigClass2)); // แก้ไข: เขียนเต็มขนาดวัตถุ
                // บังคับอ่านข้อมูลจากแรมจริงผ่านตัวแปรถังขยะระดับ Global
                local_sum += reinterpret_cast<uint8_t*>(ptr)[sizeof(BigClass2) - 1]; 
                test_ptrs.push_back(ptr);
            }
        }

        // ย้ายการ free ออกมาข้างนอก เพื่อตัดระบบ Fastpath ฟรีแล้วจองใหม่ทันที
        for (BigClass2* ptr : test_ptrs) allocator.free(ptr);
        for (BigClass0* ptr : ptrs) { if (ptr) allocator.free(ptr); }
    }

    anti_o3_sink = local_sum;
    auto end_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end_time - start_time).count();
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Hardcore Anti-O3 Benchmark: True Memory Bottleneck\n";
    std::cout << "========================================================\n\n";

    double malloc_time = benchmarkLargeMallocAntiO3();
    double palalloc_time = benchmarkLargePalallocAntiO3();
    
    std::cout << "[std::malloc] Total Time: " << std::fixed << std::setprecision(2) << malloc_time << " ms\n";
    std::cout << "[Palalloc]    Total Time: " << std::fixed << std::setprecision(2) << palalloc_time << " ms\n\n";

    if (palalloc_time > malloc_time) {
        std::cout << "💥 SUCCESS: Malloc wins! Palalloc is " << (palalloc_time / malloc_time) << "x SLOWER.\n";
    } else {
        std::cout << "⚠️ Palalloc still wins by " << (malloc_time / palalloc_time) << "x. (Fastpath is too strong)\n";
    }
    std::cout << "========================================================\n";
    return 0;
}