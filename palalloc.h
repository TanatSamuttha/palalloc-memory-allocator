#ifndef PALALLOC_H
#define PALALLOC_H

#include <stdint.h>
#include <stdlib.h>

struct Palalloc
{
    uint8_t **heads;
    uint32_t *sizeClasses;
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

    uint8_t **poolsBuffer = (uint8_t**)malloc(poolObject->mCap * sizeof(uint8_t**));
    uint32_t *sizeClassesBuffer = (uint32_t*)malloc(poolObject->mCap * sizeof(uint32_t*));
    uint8_t **headsBuffer = (uint8_t**)malloc(poolObject->mCap * sizeof(uint8_t**));

    for (int i = 0; i < poolObject->mSize; ++i)
    {
        headsBuffer[i] = poolObject->heads[i];
        sizeClassesBuffer[i] = poolObject->sizeClasses[i];
    }

    free(poolObject->heads);
    free(poolObject->sizeClasses);

    poolObject->heads = headsBuffer;
    poolObject->sizeClasses = sizeClassesBuffer;
}

void pal_newPool (Palalloc* poolObject, uint8_t **resPtr, uint32_t size, uint32_t idx)
{
    uint32_t poolSize = pal_max(4096, size * 16);
    *resPtr = (uint8_t*)malloc(poolSize);
    poolObject->heads[idx] = (uint8_t*)(resPtr + size);
    poolObject->sizeClasses[idx] = size;

    for (int i = size; i < poolSize - size; i += size)
    {
        *(uint8_t**)(poolObject->heads[idx] + i) = (uint8_t*)(poolObject->heads[idx] + i + size);
    }

    *(uint8_t**)(resPtr + poolSize - size) = nullptr;
}

void* pal_alloc (Palalloc* poolObject, uint32_t size)
{
    size = pal_nextPow2(size);

    uint32_t idx;
    for (idx = 0; idx < poolObject->mSize; ++idx)
    {
        if (size == poolObject->sizeClasses[idx])
            break;
    }

    uint8_t *resPtr = nullptr;

    if (idx < poolObject->mSize)
    {
        if (size == poolObject->sizeClasses[idx])
        {
            if (poolObject->heads[idx] != nullptr)
            {
                resPtr = poolObject->heads[idx];
                poolObject->heads[idx] = *(uint8_t**)poolObject->heads[idx];

                if (poolObject->heads[idx] != nullptr)
                {
                    poolObject->heads[idx] = nullptr;
                }
            }
            else
            {
                pal_newPool(poolObject, &resPtr, size, idx);
            }
        }
        else
        {
            ++poolObject->mSize;
            pal_ensureCap(poolObject);

            for (int i = poolObject->mSize - 1; i > idx; --i)
            {
                poolObject->heads[i] = poolObject->heads[i - 1];
                poolObject->sizeClasses[i] = poolObject->sizeClasses[i - 1];
            }

            pal_newPool(poolObject, &resPtr, size, idx);
        }
    }
    else
    {
        ++poolObject->mSize;
        pal_ensureCap(poolObject);

        pal_newPool(poolObject, &resPtr, size, idx);
    }

    return (void*)resPtr;
}

#endif