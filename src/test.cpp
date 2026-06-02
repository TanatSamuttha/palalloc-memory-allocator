#include "paralloc.h"
#include <iostream>

int main(){
    paralloc::init();

    int* ptr = paralloc::paralloc<int>();

    std::cout << "Test assign value to memmory\n";
    
    std::cout << ptr << " = " << *ptr << '\n';

    *ptr = 5;
    std::cout << ptr << " = "  << *ptr << '\n';

    *ptr = 24;
    std::cout << ptr << " = "  << *ptr << '\n';

    std::cout << "Test free memmory\n";

    paralloc::free<int>(ptr);
    std::cout << ptr << " = "  << *ptr << '\n';

    int* ptr2 = paralloc::paralloc<int>();
    std::cout << ptr << " = " << *ptr << '\n';

    return 0;
}