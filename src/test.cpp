#include "palalloc.h"
#include <iostream>

int main(){
    Palalloc allocator(1, 64 * 9);

    std::cout << Palalloc::calculateMinPages(64 * 8);

    return 0;
}