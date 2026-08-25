#include <iostream>
#include <vector>
#include "palalloc.h"

Palalloc pool = pal_create();

void test1 ()
{
    std::cout << "Test1 Linking: ";
    pal_destroy(&pool);
    pal_init(&pool);
    void* ptr = pal_alloc(&pool, 8);

    void* past = pool.heads[0];
    while (true)
    {
        void* curr = *(void**)past;
        if (curr == nullptr)
        {
            std::cout << "Pass\n";
            break;
        }
        else if ((uint8_t*)curr - (uint8_t*)past != 8)
        {
            std::cout << "Fail\n";
            break;;
        }
        past = curr;
    }
    pal_destroy(&pool);
}

void test2 ()
{
    std::cout << "Test2 Pool overflows: ";
    pal_destroy(&pool);
    pal_init(&pool);
    std::vector<void*> ptrs;
    for (int i = 0; i < 512; ++i)
    {
        void* ptr = pal_alloc(&pool, 8);
        if (!ptrs.empty() && (uint8_t*)ptr - (uint8_t*)*(ptrs.end() - 1) != 8)
        {
            std::cout << "Fail distance between pointers, distance: " << (uint8_t*)ptr - (uint8_t*)*(ptrs.end() - 1) << '\n';
            return;
        }
        ptrs.push_back(ptr);
    }
    void* ptr = pal_alloc(&pool, 8);
    void* headPtr = (void*)((uint8_t*)ptr + 8);
    void* nextPtr = *(void**)(headPtr);
    if ((uint8_t*)nextPtr - (uint8_t*)headPtr == 8)
        std::cout << "Pass\n";
    else
        std::cout << "Fail new pool linking\n";

    pal_destroy(&pool);
}

void test3 ()
{
    std::cout << "Test3 Free: ";
    pal_destroy(&pool);
    pal_init(&pool);

    void* ptr1 = pal_alloc(&pool, 8);
    pal_free(&pool, ptr1, 8);
    void* ptr2 = pal_alloc(&pool, 8);
    if (ptr1 != ptr2)
    {
        std::cout << "Fail can't reuse free address in LIFO order\n";
        return;
    }

    for (int i = 0; i < 511; ++i)
    {
        void* ptr = pal_alloc(&pool, 8);
    }

    void* ptr3 = pal_alloc(&pool, 8);
    void* ptr4 = pal_alloc(&pool, 8);
    pal_free(&pool, ptr4, 8);
    pal_free(&pool, ptr3, 8);
    void* ptr5 = pal_alloc(&pool, 8);
    void* ptr6 = pal_alloc(&pool, 8);
    if (ptr3 != ptr5)
    {
        std::cout << "Fail can't reuse free address at pool edge (ptr3 != ptr5)\n";
    }
    if (ptr4 != ptr6)
    {
        std::cout << "Fail can't reuse free address at pool edge (ptr4 != ptr6)\n";
    }

    std::cout << "Pass\n";

    pal_destroy(&pool);
}

void test4 ()
{
    std::cout << "Test4 Split: ";
    pal_destroy(&pool);
    pal_init(&pool);

    for (int i = 0; i < 512; ++i)
    {
        void* ptr = pal_alloc(&pool, 8);
    }
    void* ptr16 = pal_alloc(&pool, 16);
    void* ptr8_1 = pal_alloc(&pool, 8);
    void* ptr8_2 = pal_alloc(&pool, 8);
    if ((uint8_t*)ptr16 + 16 != (uint8_t*)(ptr8_1))
    {
        std::cout << "Fail can't use first part of stealed\n";
        return;
    }
    if ((uint8_t*)ptr16 + 24 != (uint8_t*)(ptr8_2))
    {
        std::cout << "Fail can't use second part of stealed\n";
        return;
    }

    std::cout << "Pass\n";

    pal_destroy(&pool);
}

int main ()
{
    test1();
    test2();
    test3();
    test4();

    return 0;
}