#include "paralloc.h"
#include <iostream>

int main(){
    paralloc::init();

    // long long int* ptr = paralloc::paralloc<long long int>();

    // std::cout << "Test assign value to memmory\n";
    
    // std::cout << ptr << " = " << *ptr << '\n';

    // *ptr = 5;
    // std::cout << ptr << " = "  << *ptr << '\n';

    // *ptr = 24;
    // std::cout << ptr << " = "  << *ptr << '\n';

    // std::cout << "Test free memmory\n";

    // paralloc::free<long long int>(ptr);
    // std::cout << ptr << " = "  << *ptr << '\n';

    // long long int* ptr2 = paralloc::paralloc<long long int>();
    // std::cout << ptr << " = " << *ptr << '\n';

    return 0;
}