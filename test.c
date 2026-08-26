#include <stdio.h>
#include <stdbool.h>
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
    if ((uint8_t*)nextPtr - (uint8_t*)headPtr != 8)
        printf("Fail new pool linking\n");

    printf("Pass\n");

    pal_destroy(pool);
}

void test3 (Palalloc* pool)
{
    printf("Test3 Free: ");
    pal_destroy(pool);
    pal_init(pool);

    void* ptr1 = pal_alloc(pool, 8);
    pal_free(pool, ptr1, 8);
    void* ptr2 = pal_alloc(pool, 8);
    if (ptr1 != ptr2)
    {
        printf("Fail can't reuse free address in LIFO order\n");
        return;
    }

    for (int i = 0; i < 511; ++i)
    {
        void* ptr = pal_alloc(pool, 8);
    }

    void* ptr3 = pal_alloc(pool, 8);
    void* ptr4 = pal_alloc(pool, 8);
    pal_free(pool, ptr4, 8);
    pal_free(pool, ptr3, 8);
    void* ptr5 = pal_alloc(pool, 8);
    void* ptr6 = pal_alloc(pool, 8);
    if (ptr3 != ptr5)
    {
        printf("Fail can't reuse free address at pool edge (ptr3 != ptr5)\n");
    }
    if (ptr4 != ptr6)
    {
        printf("Fail can't reuse free address at pool edge (ptr4 != ptr6)\n");
    }

    printf("Pass\n");

    pal_destroy(pool);
}

void test4 (Palalloc* pool)
{
    printf("Test4 Split: ");
    pal_destroy(pool);
    pal_init(pool);

    for (int i = 0; i < 512; ++i)
    {
        void* ptr = pal_alloc(pool, 8);
    }
    void* ptr16 = pal_alloc(pool, 16);
    void* ptr8_1 = pal_alloc(pool, 8);
    void* ptr8_2 = pal_alloc(pool, 8);
    if ((uint8_t*)ptr16 + 16 != (uint8_t*)(ptr8_1))
    {
        printf("Fail can't use first part of stealed\n");
        return;
    }
    if ((uint8_t*)ptr16 + 24 != (uint8_t*)(ptr8_2))
    {
        printf("Fail can't use second part of stealed\n");
        return;
    }

    for (int i = 0; i < 254; ++i)
    {
        void* ptr = pal_alloc(pool, 16);
    }

    void* ptr32 = pal_alloc(pool, 32);
    void* ptr8 = pal_alloc(pool, 8);

    if ((uint8_t*)ptr32 + 32 != (uint8_t*)ptr8)
    {
        printf("Fail can't steal not first next\n");
        return;
    }

    printf("Pass\n");

    pal_destroy(pool);
}

int main ()
{
    Palalloc pool = pal_create();

    test1(&pool);
    test2(&pool);
    test3(&pool);
    test4(&pool);

    return 0;
}