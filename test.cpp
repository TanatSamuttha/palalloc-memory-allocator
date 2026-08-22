#include <iostream>
#include <vector>
#include "palalloc.h"

Palalloc pool = pal_create();

void test1 ()
{
    std::cout << "Test1 Linking: ";
    pal_destroy(&pool);
    pal_init(&pool);
    void* ptr = pal_alloc(&pool, 7);

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
    bool fail = false;
    pal_destroy(&pool);
    pal_init(&pool);
    std::vector<void*> ptrs;
    for (int i = 0; i < 512; ++i)
    {
        void* ptr = pal_alloc(&pool, 7);
        if (!ptrs.empty() && (uint8_t*)ptr - (uint8_t*)*(ptrs.end() - 1) != 8)
        {
            std::cout << "Fail distance between pointers, distance: " << (uint8_t*)ptr - (uint8_t*)*(ptrs.end() - 1) << '\n';
            return;
        }
        ptrs.push_back(ptr);
    }
    void* ptr = pal_alloc(&pool, 7);
    void* headPtr = (void*)((uint8_t*)ptr + 8);
    void* nextPtr = *(void**)(headPtr);
    if ((uint8_t*)nextPtr - (uint8_t*)headPtr == 8)
        std::cout << "Pass\n";
    else
        std::cout << "Fail new pool linking\n";

    pal_destroy(&pool);
}

int main ()
{
    test1();
    test2();

    return 0;
}