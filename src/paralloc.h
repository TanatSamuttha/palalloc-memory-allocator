#ifndef PARALLOC_H
#define PARALLOC_H

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace paralloc{

    extern uint8_t* buffer;

    extern uint16_t map[4096];

    /*
        size 2 bytes is located at index 0
        size 4 bytes is located a index 1
        size 8 bytes is located a index 2
        size 16 bytes is located a index 3

        hashed by count trail zero and decrease by 1
    */
    extern uint16_t begin[4]; // Value assigned in .cpp file {0, 2048, 3072, 3584}
    extern uint8_t* head[4];
    extern uint16_t bytesleft[4]; // Value assigned in .cpp file {2048, 1024, 512, 512}

    inline void connect(uint8_t size);

    inline void init(){
        buffer = static_cast<uint8_t*>(std::malloc(4096));

        connect(8);
        connect(16);
        connect(32);
        connect(64);
    }

    inline void connect(uint8_t size){
        int sizeIdx = __builtin_ctz(size) - 3;

        uint16_t beginPad = begin[sizeIdx];
        uint8_t* headPtr = head[sizeIdx] = buffer + beginPad;
        uint8_t* ptr = headPtr;
        int16_t chunkSize = bytesleft[sizeIdx];

        while(ptr < headPtr + chunkSize){
            *reinterpret_cast<uint8_t**>(ptr) = ptr + size;
            ptr += size;
        }
        *reinterpret_cast<uint8_t**>(ptr) = nullptr;
    }
    
    // template<typename T>
    // inline T* paralloc(){
    //     constexpr int size = sizeof(T);
    //     constexpr int sizeIdx = __builtin_ctz(size) - 3;
    //     if(size > bytesleft[sizeIdx]){
    //         return static_cast<T*>(std::malloc(size));
    //     }
    //     void* ptr = static_cast<uint8_t*>(buffer) + head[sizeIdx];
    //     head[sizeIdx] = map[head[sizeIdx]];
    //     bytesleft[sizeIdx] -= size;
    //     return static_cast<T*>(ptr);
    // }

    // template<typename T>
    // inline T* malloc(){
    //     constexpr int size = sizeof(T);
    //     if (size == 2 || size == 4 || size == 8 || size == 16){
    //         return paralloc<T>();
    //     }
    //     return static_cast<T*>(std::malloc(size));
    // }

    // template<typename T>
    // inline void free(T* ptr){
    //     constexpr int size = sizeof(T);
    //     if (size != 2 && size != 4 && size != 8 && size != 16){
    //         std::free(ptr);
    //         return;
    //     }        
        
    //     uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);
    //     uint8_t* bufferByte = reinterpret_cast<uint8_t*>(buffer);

    //     if(ptrByte < bufferByte || ptrByte >= bufferByte + 4096){
    //         std::free(ptr);
    //         return;
    //     }

    //     constexpr int sizeIdx = __builtin_ctz(size) - 3;
    //     int headIdx = head[sizeIdx];
    //     int ptrIdx = ptrByte - bufferByte;

    //     map[ptrIdx] = headIdx;
    //     head[sizeIdx] = ptrIdx;

    //     bytesleft[sizeIdx] += size;
    // }
}

#endif