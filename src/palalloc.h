#ifndef PALALLOC_H
#define PALALLOC_H

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

class Palalloc{
private:
    uint8_t* pool = nullptr;

    size_t poolSize;
    
    const size_t INVALID = static_cast<size_t>(-1);

    size_t encodeSub;

    /*
        size 8 bytes is located at index 0
        size 16 bytes is located a index 1
        size 32 bytes is located a index 2
        size 64 bytes is located a index 3

        encoded by count trail zero and decrease by 3
    */
    size_t head[4];
    size_t virgin[4];
    size_t tail[4];
    size_t sizeClass[4];

    bool firstTime = true;

private:
    #ifdef _MSC_VER
        #define PAL_FORCE_INLINE __forceinline
    #else
        #define PAL_FORCE_INLINE inline __attribute__((always_inline))
    #endif
    size_t fitSize(size_t size) noexcept {
        return (size > sizeClass[3]) ? INVALID : sizeClass[(size > sizeClass[0]) + (size > sizeClass[1]) + (size > sizeClass[2])];
    }

    inline size_t combine(size_t size, size_t blocks){
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        size_t requirBytes = static_cast<size_t>(size) * blocks;

        if(virgin[sizeIdx] + requirBytes <= tail[sizeIdx] + 1){
            size_t allocIdx = tail[sizeIdx] - requirBytes + 1;
            tail[sizeIdx] -= requirBytes;

            return allocIdx;
        }
        else{
            if(size <= sizeClass[0]) return INVALID;
            else return combine((size >> 1), (blocks << 1));
        }
    }

    inline size_t split(size_t size){
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        size_t blockStart = INVALID;

        if(virgin[sizeIdx] + size <= tail[sizeIdx] + 1){
            blockStart = tail[sizeIdx] - size + 1;
            tail[sizeIdx] -= size;
        }
        else if(size < sizeClass[3]){
            blockStart = split(size << 1); 
        }

        if(blockStart != INVALID){
            size_t subSize = size >> 1;
            size_t frontBlock = blockStart;
            size_t backBlock = blockStart + subSize;

            uint8_t* frontPtr = pool + frontBlock;
            uint8_t* requesterHeadPtr = (head[sizeIdx - 1] != INVALID) ? (pool + head[sizeIdx - 1]) : nullptr;
            
            *reinterpret_cast<uint8_t**>(frontPtr) = requesterHeadPtr;
            head[sizeIdx - 1] = frontBlock;

            return backBlock;
        }

        return INVALID;
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
    inline Palalloc(size_t pages, size_t maxSize){
        poolSize = pages * 4096;

        if(maxSize > (poolSize >> 3)){
            size_t minPages = calculateMinPages(maxSize);
            throw std::invalid_argument("maxSize exceeds the allowed limit is (pages * 4096) / 8 you need atleast " + std::to_string(minPages) + ((minPages > 1)? " pages": "page"));
        }

        maxSize = std::max(maxSize, (size_t)64);

        if ((maxSize & (maxSize - 1)) != 0) {
            maxSize--;
            maxSize |= maxSize >> 1;
            maxSize |= maxSize >> 2;
            maxSize |= maxSize >> 4;
            maxSize |= maxSize >> 8;
            maxSize |= maxSize >> 16;
            #if SIZE_MAX > 0xFFFFFFFFULL
                maxSize |= maxSize >> 32;
            #endif
            maxSize++;
        }

        sizeClass[0] = (maxSize >> 3);
        sizeClass[1] = (maxSize >> 2);
        sizeClass[2] = (maxSize >> 1);
        sizeClass[3] = maxSize;

        encodeSub = ctz(static_cast<uint32_t>(sizeClass[0]));
    }

    inline ~Palalloc(){
        std::free(pool);
    }

    inline Palalloc(const Palalloc&) = delete;

    inline Palalloc& operator=(const Palalloc&) = delete;

    inline void init(){
        if(!firstTime) return;
        pool = static_cast<uint8_t*>(std::malloc(poolSize));
        reset();
        firstTime = false;
    }

    inline void* getPool(){
        return static_cast<void*>(pool);
    }

    template<typename T>
    inline size_t getHead() noexcept {
        size_t size = fitSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return head[sizeIdx];
    }

    template<typename T>
    inline size_t getTail() noexcept {
        size_t size = fitSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return tail[sizeIdx];
    }

    template<typename T>
    inline size_t getVirgin() noexcept {
        size_t size = fitSize(sizeof(T));
        if(size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return virgin[sizeIdx];
    }

    static inline size_t calculateMinPages(size_t maxSize) noexcept {
        size_t reqPoolSize = maxSize << 3;
        return (reqPoolSize + 4095) >> 12;
    }

    template<typename T>
    inline T* alloc(){
        if(firstTime) init();

        size_t size = fitSize(sizeof(T));
        if(size == INVALID) return nullptr;

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;

        if(head[sizeIdx] != INVALID){
            void* ptr = pool + head[sizeIdx];
            uint8_t* next = *reinterpret_cast<uint8_t**>(ptr);
            head[sizeIdx] = (next == nullptr) ? INVALID : static_cast<size_t>(next - pool);
            return reinterpret_cast<T*>(ptr);
        }

        void* newPtr = loadChunk(sizeIdx, size);
        if(newPtr != nullptr){
            return reinterpret_cast<T*>(newPtr);
        }

        size_t combineIdx = (size > sizeClass[0]) ? combine(size >> 1, 2) : INVALID;
        if(combineIdx != INVALID){
            return reinterpret_cast<T*>(pool + combineIdx);
        }

        size_t splitIdx = (size < sizeClass[3]) ? split(size << 1) : INVALID;
        if(splitIdx != INVALID){
            return reinterpret_cast<T*>(pool + splitIdx);
        }
        
        return nullptr;
    }

    template<typename T>
    inline T* galloc(){
        size_t size = fitSize(sizeof(T));
        if(sizeof(T) <= sizeClass[3]){
            T* ptr = alloc<T>();
            if(ptr != nullptr){
                return ptr;
            }
            return static_cast<T*>(std::malloc(size));
        }
        return static_cast<T*>(std::malloc(sizeof(T)));
    }

    template<typename T>
    inline void free(T* ptr){
        size_t size = fitSize(sizeof(T));
        if (size > sizeClass[3]){
            std::free(ptr);
            return;
        }
        
        uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);

        if(ptrByte < pool || ptrByte >= pool + poolSize){
            std::free(ptr);
            return;
        }

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;

        uint8_t* headPtr = (head[sizeIdx] != INVALID)? pool + head[sizeIdx] : nullptr;

        *reinterpret_cast<uint8_t**>(ptrByte) = headPtr;
        head[sizeIdx] = static_cast<size_t>(ptrByte - pool);
    }

    inline void reset() noexcept {
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

    inline void hardReset(){
        std::free(pool);
        pool = nullptr;
        firstTime = true;
        reset();
    }
};

#endif