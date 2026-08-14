#include <iostream>
#include "palalloc.h"

int main ()
{
    Palalloc pool;

    void *ptr = pal_alloc(&pool, 16);
    std::cout << ptr << '\n';

    // void *past = pool.heads[0];
    // while (true)
    // {
    //     void *curr = *(void**)past;
    //     if (curr == nullptr)
    //     {
    //         std::cout << "Pass\n";
    //     }
    //     if ((uint8_t*)curr - (uint8_t*)past != 16)
    //     {
    //         std::cout << "Fail\n";
    //         return 0;
    //     }
    // }

    return 0;
}