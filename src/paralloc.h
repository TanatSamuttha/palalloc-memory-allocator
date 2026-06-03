#ifndef PARALLOC_H
#define PARALLOC_H

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace paralloc{

    extern uint8_t* buffer;

    /*
        size 8 bytes is located at index 0
        size 16 bytes is located a index 1
        size 32 bytes is located a index 2
        size 64 bytes is located a index 3

        hashed by count trail zero and decrease by 1
    */
    extern uint16_t head[4]; // Value assigned in .cpp file {0, 2048, 3072, 3584}
    extern uint16_t virgin[4]; // Value assigned in .cpp file {0, 2048, 3072, 3584}
    extern uint16_t tail[4]; // Value assigned in .cpp file {2047, 3071, 3583, 4095}

    extern const uint16_t INVALID; // Value assigned in .cpp file 0xFFFF

    inline constexpr int findSize(size_t size){
        if(size <= 8) return 8;
        if(size <= 16) return 16;
        if(size <= 32) return 32;
        if(size <= 64) return 64;

        return -1;
    }

    inline void connect(uint8_t size, uint16_t chunkSize){
        int sizeIdx = __builtin_ctz(size) - 3;
        
        uint16_t headPad = head[sizeIdx];
        uint8_t* headPtr = buffer + headPad;
        uint8_t* ptr = buffer + headPad;
        
        while(ptr + size < headPtr + chunkSize){
            *reinterpret_cast<uint8_t**>(ptr) = ptr + size;
            ptr += size;
        }
        
        *reinterpret_cast<uint8_t**>(ptr) = nullptr;
    }

    inline void init(){
        buffer = static_cast<uint8_t*>(std::malloc(4096));

        connect(8, 2048);
        connect(16, 1024);
        connect(32, 512);
        connect(64, 512);
    }

    inline uint16_t combine(uint8_t size){
        int sizeIdx = __builtin_ctz(size) - 3;
        uint8_t size2 = size + size;
        if(virgin[sizeIdx] <= tail[sizeIdx] - size2 + 1){
            tail[sizeIdx] -= size2;

            uint8_t* ptr = buffer + (tail[sizeIdx] - size + 1);
            *reinterpret_cast<uint8_t**>(ptr) = nullptr;

            return tail[sizeIdx] + 1;
        }
        else{
            if(size <= 8){
                return INVALID;
            }
            else{
                return combine(size >> 1);
            }
        }
    }
    
    template<typename T>
    inline T* paralloc(){
        constexpr int size = findSize(sizeof(T));
        constexpr int sizeIdx = __builtin_ctz(size) - 3;

        if(head[sizeIdx] == INVALID){
            int16_t combineIdx = combine(size >> 1);
            if(combineIdx == INVALID) return static_cast<T*>(std::malloc(size));
            else return reinterpret_cast<T*>(buffer + combineIdx);
        }

        void* ptr = buffer + head[sizeIdx];
        if(head[sizeIdx] == virgin[sizeIdx]) virgin[sizeIdx] += size;

        uint8_t* next = *reinterpret_cast<uint8_t**>(ptr);
        head[sizeIdx] = (next == nullptr)? INVALID : next - buffer;

        return static_cast<T*>(ptr);
    }

    template<typename T>
    inline T* malloc(){
        constexpr int size = findSize(sizeof(T));
        if(size <= 64){
            return paralloc<T>();
        }
        return static_cast<T*>(std::malloc(sizeof(T)));
    }

    template<typename T>
    inline void free(T* ptr){
        constexpr int size = findSize(sizeof(T));
        if (size > 64){
            std::free(ptr);
            return;
        }        
        
        uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);

        if(ptrByte < buffer || ptrByte >= buffer + 4096){
            std::free(ptr);
            return;
        }

        constexpr int sizeIdx = __builtin_ctz(size) - 3;

        uint8_t* headPtr = (head[sizeIdx] != INVALID)? buffer + head[sizeIdx] : nullptr;

        *reinterpret_cast<uint8_t**>(ptrByte) = headPtr;
        head[sizeIdx] = ptrByte - buffer;
        if(head[sizeIdx] == virgin[sizeIdx] - size) virgin[sizeIdx] -= size;
    }
}

#endif