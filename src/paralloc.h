#ifndef PARALLOC_H
#define PARALLOC_H

#include <cstdint>
#include <cstdlib>

class Paralloc{
private:
    uint8_t* buffer;

    /*
        size 8 bytes is located at index 0
        size 16 bytes is located a index 1
        size 32 bytes is located a index 2
        size 64 bytes is located a index 3

        hashed by count trail zero and decrease by 3
    */
    uint16_t head[4] = {0, 2048, 3072, 3584};
    uint16_t virgin[4] = {0, 2048, 3072, 3584};
    uint16_t tail[4] = {2047, 3071, 3583, 4095};

    const uint16_t INVALID = 0xFFFF;

    bool firstTime = true;

private:
    inline int findSize(size_t size){
        if(size <= 8) return 8;
        if(size <= 16) return 16;
        if(size <= 32) return 32;
        if(size <= 64) return 64;

        return INVALID;
    }

    inline void connect(uint8_t size, uint16_t chunkSize){
        int sizeIdx = ctz(size) - 3;
        
        uint16_t headPad = head[sizeIdx];
        uint8_t* headPtr = buffer + headPad;
        uint8_t* ptr = buffer + headPad;
        
        while(ptr + size < headPtr + chunkSize){
            *(uint8_t**)ptr = ptr + size;
            ptr += size;
        }
        
        *(uint8_t**)ptr = nullptr;
    }

    inline uint16_t combine(uint8_t size){
        int sizeIdx = ctz(size) - 3;
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
    
    #ifdef _MSC_VER
    #include <intrin.h>

    inline int ctz(unsigned int x){
        unsigned long idx;
        _BitScanForward(&idx, x);
        return (int)idx;
    }
    #else
    inline int ctz(unsigned int x){
        return __builtin_ctz(x);
    }
    #endif

public:
    ~Paralloc(){
        std::free(buffer);
    }

    Paralloc(const Paralloc&) = delete;
    Paralloc& operator=(const Paralloc&) = delete;

    template<typename T>
    inline T* alloc(){
        if(firstTime){
            buffer = (uint8_t*)std::malloc(4096);
            connect(8, 2048);
            connect(16, 1024);
            connect(32, 512);
            connect(64, 512);
            firstTime = false;
        }

        int size = findSize(sizeof(T));
        int sizeIdx = ctz(size) - 3;

        if(head[sizeIdx] == INVALID){
            int16_t combineIdx = combine(size >> 1);
            if(combineIdx == INVALID) return static_cast<T*>(std::malloc(size));
            else return reinterpret_cast<T*>(buffer + combineIdx);
        }

        void* ptr = buffer + head[sizeIdx];
        if(head[sizeIdx] == virgin[sizeIdx]) virgin[sizeIdx] += size;

        uint8_t* next = *(uint8_t**)ptr;
        head[sizeIdx] = (next == nullptr)? INVALID : next - buffer;

        return (T*)ptr;
    }

    template<typename T>
    inline T* galloc(){
        int size = findSize(sizeof(T));
        if(sizeof(T) <= 64){
            return alloc<T>();
        }
        return (T*)std::malloc(sizeof(T));
    }

    template<typename T>
    inline void free(T* ptr){
        int size = findSize(sizeof(T));
        if (size > 64){
            std::free(ptr);
            return;
        }
        
        uint8_t* ptrByte = (uint8_t*)ptr;

        if(ptrByte < buffer || ptrByte >= buffer + 4096){
            std::free(ptr);
            return;
        }

        int sizeIdx = ctz(size) - 3;

        uint8_t* headPtr = (head[sizeIdx] != INVALID)? buffer + head[sizeIdx] : nullptr;

        *(uint8_t**)ptrByte = headPtr;
        head[sizeIdx] = ptrByte - buffer;
        if(head[sizeIdx] == virgin[sizeIdx] - size) virgin[sizeIdx] -= size;
    }
};


#endif