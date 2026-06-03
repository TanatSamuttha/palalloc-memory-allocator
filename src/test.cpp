#include "paralloc.h"
#include <iostream>

int main(){
    paralloc::init();

    long long int* ptr = paralloc::paralloc<long long int>();

    std::cout << "Test assign value to memmory\n";
    
    std::cout << ptr << " = " << *ptr << '\n';

    *ptr = 5;
    std::cout << ptr << " = "  << *ptr << '\n';

    *ptr = 24;
    std::cout << ptr << " = "  << *ptr << '\n';

    std::cout << "Test unsupported size\n";

    uint32_t* ptr2 = paralloc::malloc<uint32_t>();

    std::cout << ptr2 << " = " << *ptr2 << '\n';

    *ptr2 = 64;
    std::cout << ptr2 << " = " << *ptr2 << '\n';

    std::cout << "Test free memmory\n";

    paralloc::free<long long int>(ptr);
    std::cout << ptr << " = "  << *ptr << '\n';

    long long int* ptr3 = paralloc::paralloc<long long int>();
    std::cout << ptr3 << " = " << *ptr3 << '\n';

    return 0;
}