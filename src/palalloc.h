#ifndef PALALLOC_H
#define PALALLOC_H

#include <cstdint>
#include <cstdlib>

class Palalloc{
private:
    uint8_t* pool = nullptr;

    size_t poolSize;
    
    const size_t INVALID = static_cast<size_t>(-1);

    /*
        size 8 bytes is located at index 0
        size 16 bytes is located a index 1
        size 32 bytes is located a index 2
        size 64 bytes is located a index 3

        hashed by count trail zero and decrease by 3
    */
    size_t head[4];
    size_t virgin[4];
    size_t tail[4];

    bool firstTime = true;

private:
    inline size_t findSize(size_t size){
        if(size <= 8) return 8;
        if(size <= 16) return 16;
        if(size <= 32) return 32;
        if(size <= 64) return 64;

        return INVALID;
    }

    inline size_t combine(size_t size, size_t blocks){
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;
        size_t allSize = static_cast<size_t>(size) * blocks;

        if(tail[sizeIdx] >= allSize - 1 && virgin[sizeIdx] <= tail[sizeIdx] - allSize + 1){
            size_t allocIdx = tail[sizeIdx] - allSize + 1;
            tail[sizeIdx] -= allSize;

            return allocIdx;
        }
        else{
            if(size <= 8){
                return INVALID;
            }
            else{
                return combine((size >> 1), (blocks << 1));
            }
        }
    }
    
    #ifdef _MSC_VER
    #include <intrin.h>

    inline int8_t ctz(uint32_t x){
        size_t idx;
        _BitScanForward(&idx, x);
        return static_cast<int>(idx);
    }
    #else
    inline int8_t ctz(uint32_t x){
        return __builtin_ctz(x);
    }
    #endif

    #ifdef _MSC_VER
    __declspec(noinline)
    #else
    __attribute__((noinline))
    #endif
    void* loadChunk(uint8_t sizeIdx, size_t size){
        if(virgin[sizeIdx] + size > tail[sizeIdx] + 1) return nullptr;

        uint8_t chunkSize = 16;
        size_t startVirgin = virgin[sizeIdx];
        size_t current = startVirgin;

        uint8_t* ptr = pool + current;
        size_t allocatedCount = 1;

        for(int i = 0; i < chunkSize - 1; ++i){
            if(current + size * 2 > tail[sizeIdx] + 1) break;
            *reinterpret_cast<uint8_t**>(ptr) = ptr + size;
            ptr += size;
            current += size;
            allocatedCount++;
        }
        *reinterpret_cast<uint8_t**>(ptr) = nullptr;

        virgin[sizeIdx] = current + size;

        void* result = pool + startVirgin;
        
        head[sizeIdx] = (allocatedCount > 1) ? (startVirgin + size) : INVALID;

        return result;
    }

public:

    Palalloc(size_t pages){
        poolSize = 4096 * pages;
    }

    ~Palalloc(){
        std::free(pool);
    }

    Palalloc(const Palalloc&) = delete;
    Palalloc& operator=(const Palalloc&) = delete;

    inline void* getpool(){
        return static_cast<void*>(pool);
    }

    template<typename T>
    inline size_t getHead(){
        size_t size = findSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;
        return head[sizeIdx];
    }

    template<typename T>
    inline size_t getTail(){
        size_t size = findSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;
        return tail[sizeIdx];
    }

    template<typename T>
    inline size_t getVirgin(){
        size_t size = findSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;
        return virgin[sizeIdx];
    }

    template<typename T>
    inline T* alloc(){
        if(firstTime){
            pool = static_cast<uint8_t*>(std::malloc(poolSize));
            reset();
            firstTime = false;
        }

        size_t size = findSize(sizeof(T));
        if(size == INVALID) return nullptr;

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;

        if(head[sizeIdx] != INVALID){
            void* ptr = pool + head[sizeIdx];
            uint8_t* next = *reinterpret_cast<uint8_t**>(ptr);
            head[sizeIdx] = (next == nullptr) ? INVALID : static_cast<size_t>(next - pool);
            return reinterpret_cast<T*>(ptr);
        }

        void* newPtr = loadChunk(sizeIdx, size);
        if (newPtr != nullptr) {
            return reinterpret_cast<T*>(newPtr);
        }

        size_t combineIdx = (size > 8) ? combine(size >> 1, 2) : INVALID;
        if(combineIdx != INVALID) {
            return reinterpret_cast<T*>(pool + combineIdx);
        }

        return static_cast<T*>(std::malloc(size));
    }

    template<typename T>
    inline T* galloc(){
        size_t size = findSize(sizeof(T));
        if(sizeof(T) <= 64){
            return alloc<T>();
        }
        return static_cast<T*>(std::malloc(sizeof(T)));
    }

    template<typename T>
    inline void free(T* ptr){
        size_t size = findSize(sizeof(T));
        if (size > 64){
            std::free(ptr);
            return;
        }
        
        uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);

        if(ptrByte < pool || ptrByte >= pool + poolSize){
            std::free(ptr);
            return;
        }

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - 3;

        uint8_t* headPtr = (head[sizeIdx] != INVALID)? pool + head[sizeIdx] : nullptr;

        *reinterpret_cast<uint8_t**>(ptrByte) = headPtr;
        head[sizeIdx] = static_cast<size_t>(ptrByte - pool);
    }

    void reset(){
        head[0] = head[1] = head[2] = head[3] = INVALID;

        virgin[0] = 0; 
        virgin[1] = (poolSize >> 1); // 2048 at 1 page
        virgin[2] = (poolSize >> 1) + (poolSize >> 2); // 3072 at 1 page
        virgin[3] = (poolSize >> 1) + (poolSize >> 2) + (poolSize >> 3); // 3584 at 1 page

        tail[0] = (poolSize >> 1) - 1; // 2047 at 1 page
        tail[1] = (poolSize >> 1) + (poolSize >> 2) - 1; // 3071 at 1 page
        tail[2] = (poolSize >> 1) + (poolSize >> 2) + (poolSize >> 3) - 1; // 3583 at 1 page
        tail[3] = poolSize - 1; // 4095 at 1 page
    }
};


#endif