#include "paralloc.h"
#include <iostream>

struct HugeObject {
    uint64_t a[16];  // 128 bytes
    double b[8];     // 64 bytes
    char c[32];      // 32 bytes
};

int main(){
    Paralloc paralloc;

    long long int* ptr = paralloc.alloc<long long int>();

    std::cout << "Test assign value to memmory\n";
    
    std::cout << ptr << " = " << *ptr << '\n';

    *ptr = 5;
    std::cout << ptr << " = "  << *ptr << '\n';

    *ptr = 24;
    std::cout << ptr << " = "  << *ptr << '\n';

    std::cout << "Test unsupported size\n";

    HugeObject* ptr2 = paralloc.galloc<HugeObject>();

    std::cout << ptr2 << " = " << (*ptr2).a[0] << '\n';

    (*ptr2).a[0] = 64;
    std::cout << ptr2 << " = " << (*ptr2).a[0] << '\n';

    std::cout << "Test free memmory\n";

    paralloc.free<long long int>(ptr);
    std::cout << ptr << " = "  << *ptr << '\n';

    long long int* ptr3 = paralloc.alloc<long long int>();
    std::cout << ptr3 << " = " << *ptr3 << '\n';
}