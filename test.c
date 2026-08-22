#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include "palalloc.h"

void test1 (Palalloc* pool)
{
    printf("Test1 Linking: ");
    pal_destroy(pool);
    pal_init(pool);
    void* ptr = pal_alloc(pool, 8);

    void* past = pool->heads[0];
    while (true)
    {
        void* curr = *(void**)past;
        if (curr == NULL)
        {
            printf("Pass\n");
            break;
        }
        else if ((uint8_t*)curr - (uint8_t*)past != 8)
        {
            printf("Fail\n");
            break;;
        }
        past = curr;
    }
    pal_destroy(pool);
}

void test2 (Palalloc* pool)
{
    printf("Test2 Pool overflows: ");
    bool fail = false;
    pal_destroy(pool);
    pal_init(pool);
    void* ptrs[600];
    int idx = 0;
    for (int i = 0; i < 512; ++i)
    {
        void* ptr = pal_alloc(pool, 8);
        if (idx && (uint8_t*)ptr - (uint8_t*)ptrs[idx - 1] != 8)
        {
            printf("Fail distance between pointers, distance: %d", (uint8_t*)ptr - (uint8_t*)ptrs[idx - 1]);
            return;
        }
        ptrs[idx++] = ptr;
    }
    void* ptr = pal_alloc(pool, 8);
    void* headPtr = (void*)((uint8_t*)ptr + 8);
    void* nextPtr = *(void**)(headPtr);
    if ((uint8_t*)nextPtr - (uint8_t*)headPtr == 8)
        printf("Pass\n");
    else
        printf("Fail new pool linking\n");

    pal_destroy(pool);
}

int main ()
{
    Palalloc pool = pal_create();

    test1(&pool);
    test2(&pool);

    return 0;
}