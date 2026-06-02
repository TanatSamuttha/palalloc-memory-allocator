#include "paralloc.h"
#include <iostream>

int main(){
    paralloc::init();

    int* ptr = paralloc::paralloc<int>();
    
    std::cout << *ptr << '\n';

    *ptr = 5;
    std::cout << *ptr << '\n';

    *ptr = 24;
    std::cout << *ptr << '\n';

    return 0;
}