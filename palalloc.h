#ifndef PALALLOC_H
#define PALALLOC_H

#include <stdint.h>
#include <stdlib.h>

struct Palalloc
{
    void **pools;
    uint32_t *dataClasses;
    uint32_t mSize;
    uint32_t mCap;
};

uint32_t pal_clz (uint32_t x)
{
    #if defined(_MSC_VER)
        unsigned long index;
        if (_BitScanReverse(&index, x))
            return 31u - index;
        return 32;
    #else
        if (x) return (uint32_t)__builtin_clz(x);
        return 32;
    #endif
}

uint32_t pal_max (uint32_t a, uint32_t b)
{
    if (a > b) return a;
    return b;
}

uint32_t pal_nextPow2 (uint32_t x)
{
    return 1 << (32 - pal_clz(x));
}

void pal_ensureCap (Palalloc* poolObject)
{
    if (poolObject->mSize <= poolObject->mCap)
        return;

    poolObject->mCap = pal_max(poolObject->mSize, poolObject->mCap * 2);

    void** poolsBuffer = (void**)malloc(poolObject->mCap * sizeof(void**));
    uint32_t* dataClassesBuffer = (uint32_t*)malloc(poolObject->mCap * sizeof(uint32_t*));

    for (int i = 0; i < poolObject->mSize; ++i)
    {
        poolsBuffer[i] = poolObject->pools[i];
        dataClassesBuffer[i] = poolObject->dataClasses[i];
    }

    free(poolObject->pools);
    free(poolObject->dataClasses);

    poolObject->pools = poolsBuffer;
    poolObject->dataClasses = dataClassesBuffer;
}

void* pal_alloc (Palalloc* poolObject, uint32_t size)
{
    size = pal_nextPow2(size);

    uint32_t idx;
    for (idx = 0; idx < poolObject->mSize; ++idx)
    {
        if (size == poolObject->dataClasses[idx])
            break;
    }

    if (idx == poolObject->mSize)
    {
        ++poolObject->mSize;
        pal_ensureCap(poolObject);

        poolObject->pools[idx] = malloc(pal_max(4096, size * 16));
        poolObject->dataClasses[idx] = size;
    }

    return nullptr;
}

#endif