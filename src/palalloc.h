#ifndef PALALLOC_H
#define PALALLOC_H

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <stdexcept>

class Palalloc {
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
    struct State {
        size_t head;
        size_t virgin;
        size_t tail;
    } state[4];

    size_t sizeClasses[4];

    bool firstTime = true;

private:
    #ifdef _MSC_VER
        #define PAL_FORCE_INLINE __forceinline
    #else
        #define PAL_FORCE_INLINE inline __attribute__((always_inline))
    #endif
    size_t fitSize(size_t size) noexcept {
        return (size > sizeClasses[3]) ? INVALID : sizeClasses[(size > sizeClasses[0]) + (size > sizeClasses[1]) + (size > sizeClasses[2])];
    }

    inline size_t combine(size_t size, size_t blocks){
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        size_t requirBytes = static_cast<size_t>(size) * blocks;

        if (state[sizeIdx].virgin + requirBytes <= state[sizeIdx].tail + 1) {
            size_t allocIdx = state[sizeIdx].tail - requirBytes + 1;
            state[sizeIdx].tail -= requirBytes;

            return allocIdx;
        }
        else {
            if(size <= sizeClasses[0]) return INVALID;
            else return combine((size >> 1), (blocks << 1));
        }
    }

    inline size_t split(size_t size) {
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        size_t blockStart = INVALID;

        if (state[sizeIdx].virgin + size <= state[sizeIdx].tail + 1) {
            blockStart = state[sizeIdx].tail - size + 1;
            state[sizeIdx].tail -= size;
        }
        else if (size < sizeClasses[3]) {
            blockStart = split(size << 1); 
        }

        if (blockStart != INVALID) {
            size_t subSize = size >> 1;
            size_t frontBlock = blockStart;
            size_t backBlock = blockStart + subSize;

            uint8_t* frontPtr = pool + frontBlock;
            uint8_t* requesterHeadPtr = (state[sizeIdx - 1].head != INVALID) ? (pool + state[sizeIdx - 1].head) : nullptr;
            
            *reinterpret_cast<uint8_t**>(frontPtr) = requesterHeadPtr;
            state[sizeIdx - 1].head = frontBlock;

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
    inline int8_t ctz(uint32_t x) {
        return __builtin_ctz(x);
    }
    #endif

    #ifdef _MSC_VER
    __declspec(noinline)
    #else
    __attribute__((noinline))
    #endif
    void* loadChunk(uint8_t sizeIdx, size_t size) {
        if (state[sizeIdx].virgin + size > state[sizeIdx].tail + 1) return nullptr;

        uint8_t chunkSize = 16;
        size_t startVirgin = state[sizeIdx].virgin;
        size_t current = startVirgin;

        uint8_t* ptr = pool + current;
        size_t allocatedCount = 1;

        for (int i = 0; i < chunkSize - 1; ++i) {
            if(current + size * 2 > state[sizeIdx].tail + 1) break;
            *reinterpret_cast<uint8_t**>(ptr) = ptr + size;
            ptr += size;
            current += size;
            allocatedCount++;
        }
        *reinterpret_cast<uint8_t**>(ptr) = nullptr;

        state[sizeIdx].virgin = current + size;

        void* result = pool + startVirgin;
        
        state[sizeIdx].head = (allocatedCount > 1) ? (startVirgin + size) : INVALID;

        return result;
    }

public:
    inline Palalloc(size_t pages, size_t maxSize) {
        poolSize = pages * 4096;

        if (maxSize > (poolSize >> 3)) {
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

        sizeClasses[0] = (maxSize >> 3);
        sizeClasses[1] = (maxSize >> 2);
        sizeClasses[2] = (maxSize >> 1);
        sizeClasses[3] = maxSize;

        encodeSub = ctz(static_cast<uint32_t>(sizeClasses[0]));
    }

    inline ~Palalloc() {
        std::free(pool);
    }

    inline Palalloc(const Palalloc&) = delete;

    inline Palalloc& operator=(const Palalloc&) = delete;

    inline void init() {
        if (!firstTime) return;
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
        if (size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return state[sizeIdx].head;
    }

    template<typename T>
    inline size_t getTail() noexcept {
        size_t size = fitSize(sizeof(T));
        if (size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return state[sizeIdx].tail;
    }

    template<typename T>
    inline size_t getVirgin() noexcept {
        size_t size = fitSize(sizeof(T));
        if (size == INVALID) return INVALID;
        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;
        return state[sizeIdx].virgin;
    }

    static inline size_t calculateMinPages(size_t maxSize) noexcept {
        size_t reqPoolSize = maxSize << 3;
        return (reqPoolSize + 4095) >> 12;
    }

    template<typename T>
    inline T* alloc() {
        return static_cast<T*>(alloc(sizeof(T)));
    }

    inline void* alloc(size_t bytes) {
        if (firstTime) init();

        size_t size = fitSize(bytes);
        if (size == INVALID) return nullptr;

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;

        if (state[sizeIdx].head != INVALID) {
            void* ptr = pool + state[sizeIdx].head;
            uint8_t* next = *reinterpret_cast<uint8_t**>(ptr);
            state[sizeIdx].head = (next == nullptr) ? INVALID : static_cast<size_t>(next - pool);
            return static_cast<void*>(ptr);
        }

        void* newPtr = loadChunk(sizeIdx, size);
        if (newPtr != nullptr) {
            return static_cast<void*>(newPtr);
        }

        size_t combineIdx = (size > sizeClasses[0]) ? combine(size >> 1, 2) : INVALID;
        if (combineIdx != INVALID) {
            return static_cast<void*>(pool + combineIdx);
        }

        size_t splitIdx = (size < sizeClasses[3]) ? split(size << 1) : INVALID;
        if (splitIdx != INVALID) {
            return static_cast<void*>(pool + splitIdx);
        }
        
        return nullptr;
    }

    template<typename T>
    inline T* galloc() {
        return static_cast<T*>(galloc(sizeof(T)));
    }

    inline void* galloc(size_t bytes) {
        size_t size = fitSize(bytes);
        if (size != INVALID) {
            void* ptr = alloc(size);
            if (ptr != nullptr) {
                return static_cast<void*>(ptr);
            }
            return static_cast<void*>(std::malloc(size));
        }
        return static_cast<void*>(std::malloc(bytes));
    }

    template<typename T>
    inline void free(T* ptr) {
        free(static_cast<void*>(ptr), sizeof(T));
    }

    inline void free(void* ptr, size_t size) {
        size = fitSize(size);
        if (size == INVALID) {
            std::free(ptr);
            return;
        }
        
        uint8_t* ptrByte = reinterpret_cast<uint8_t*>(ptr);

        if (ptrByte < pool || ptrByte >= pool + poolSize) {
            std::free(ptr);
            return;
        }

        uint8_t sizeIdx = ctz(static_cast<uint32_t>(size)) - encodeSub;

        uint8_t* headPtr = (state[sizeIdx].head != INVALID)? pool + state[sizeIdx].head : nullptr;

        *reinterpret_cast<uint8_t**>(ptrByte) = headPtr;
        state[sizeIdx].head = static_cast<size_t>(ptrByte - pool);
    }

    inline void reset() noexcept {
        state[0].head = state[1].head = state[2].head = state[3].head = INVALID;

        state[0].virgin = 0; 
        state[1].virgin = (poolSize >> 1); // 2048 at 1 page
        state[2].virgin = (poolSize >> 1) + (poolSize >> 2); // 3072 at 1 page
        state[3].virgin = (poolSize >> 1) + (poolSize >> 2) + (poolSize >> 3); // 3584 at 1 page

        state[0].tail = (poolSize >> 1) - 1; // 2047 at 1 page
        state[1].tail = (poolSize >> 1) + (poolSize >> 2) - 1; // 3071 at 1 page
        state[2].tail = (poolSize >> 1) + (poolSize >> 2) + (poolSize >> 3) - 1; // 3583 at 1 page
        state[3].tail = poolSize - 1; // 4095 at 1 page
    }

    inline void hardReset() {
        std::free(pool);
        pool = nullptr;
        firstTime = true;
        reset();
    }
};

#endif