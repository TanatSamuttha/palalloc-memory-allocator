#include <iostream>
#include "palalloc.h"

int main ()
{
    Palalloc pool;
    pal_init(&pool);

    void *ptr = pal_alloc(&pool, 7);

    std::cout << "Linking: ";
    void *past = pool.heads[0];
    while (true)
    {
        void *curr = *(void**)past;
        if (curr == nullptr)
        {
            std::cout << "Pass\n";
            break;
        }
        else if ((uint8_t*)curr - (uint8_t*)past != 8)
        {
            std::cout << "Fail\n";
            break;
        }
        past = curr;
    }

    return 0;
}