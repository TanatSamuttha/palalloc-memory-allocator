#include "paralloc.h"
#include <iostream>

struct bytes64{
    int8_t num[64];
};

struct bytes32{
    int8_t num[32];
};

struct bytes16{
    int8_t num[16];
};

int main(){
    Paralloc paralloc;

    // for(int i = 0; i < 9; i++){
    //     int prevtail32 = paralloc.getTail<bytes32>();
    //     bytes64* ptr = paralloc.alloc<bytes64>();
    //     int alloc = (uint8_t*)ptr - (uint8_t*)paralloc.getBuffer();
    //     int head64 = paralloc.getHead<bytes64>();
    //     int tail32 = paralloc.getTail<bytes32>();
    //     std::cout << "New allocated at " << alloc << " | 64 bytes head is " << head64 << " | previous 32 bytes tail is " << prevtail32 << " | now 32 bytes tail is " << tail32 << '\n';
    // }

    // for(int i = 0; i < 19; i++){
    //     int prevtail16 = paralloc.getTail<bytes16>();
    //     bytes64* ptr = paralloc.alloc<bytes64>();
    //     int alloc = (uint8_t*)ptr - (uint8_t*)paralloc.getBuffer();
    //     int head64 = paralloc.getHead<bytes64>();
    //     int tail16 = paralloc.getTail<bytes16>();
    //     std::cout << "New allocated at " << alloc << " | 64 bytes head is " << head64 << " | previous 16 bytes tail is " << prevtail16 << " | now 16 bytes tail is " << tail16 << '\n';
    // }

    for(int i = 0; i < 2000; i++){
        long long* ptr = paralloc.alloc<long long>();
        std::cout << sizeof(long long) << ' ' << ptr << '\n';
    }
}