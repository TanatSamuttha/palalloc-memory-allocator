#ifndef PARALLOC_H
#define PARALLOC_H

#include <cstdlib>
#include <cstdint>

namespace paralloc{

    extern void* buffer;

    extern uint16_t map[4096];

    /*
        size 2 bytes is located at index 0
        size 4 bytes is located a index 1
        size 8 bytes is located a index 2
        size 16 bytes is located a index 3

        hashed by count trail zero and decrease by 1
    */
    extern uint16_t begin[4]; // Value assigned in .cpp file {0, 2048, 3072, 3584}
    extern uint16_t bytesleft[4]; // Value assigned in .cpp file {2048, 1024, 512, 512}

    inline void connect(uint8_t size);

    inline void init(){
        buffer = std::malloc(4096);

        connect(2);
        connect(4);
        connect(8);
        connect(16);
    }

    inline void connect(uint8_t size){
        int sizeIdx = __builtin_ctz(size) - 1;
        int beginIdx = begin[sizeIdx];
        int idx = beginIdx;
        int chunkSize = bytesleft[sizeIdx];
        while(idx < beginIdx + chunkSize){
            map[idx] = idx + size;
            idx += size;
        }
    }
    
    template<typename T>
    inline T* paralloc(){
        constexpr int size = sizeof(T);
        constexpr int sizeIdx = __builtin_ctz(size) - 1;
        if(size > bytesleft[sizeIdx]){
            return static_cast<T*>(std::malloc(size));
        }
        void* ptr = static_cast<uint8_t*>(buffer) + begin[sizeIdx];
        begin[sizeIdx] = map[begin[sizeIdx]];
        bytesleft[sizeIdx] -= size;
        return static_cast<T*>(ptr);
    }

    template<typename T>
    inline T* malloc(){
        constexpr int size = sizeof(T);
        if (size == 2 || size == 4 || size == 8 || size == 16){
            return paralloc<T>();
        }
        return static_cast<T*>(std::malloc(size));
    }

    template<typename T>
    inline void free(T* ptr){
        constexpr int size = sizeof(T);
        if (size != 2 && size != 4 && size != 8 && size != 16){
            std::free(ptr);
            return;
        }        
        
        uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);
        uint8_t* bufferByte = reinterpret_cast<uint8_t*>(buffer);

        if(ptrByte < bufferByte || ptrByte >= bufferByte + 4096){
            std::free(ptr);
            return;
        }

        constexpr int sizeIdx = __builtin_ctz(size) - 1;
        int beginIdx = begin[sizeIdx];
        int ptrIdx = ptrByte - bufferByte;

        map[ptrIdx] = beginIdx;
        begin[sizeIdx] = ptrIdx;

        bytesleft[sizeIdx] += size;
    }
}

#endif