#include <cstdlib>
#include <cstdint>

namespace paralloc{

    void* buffer;

    int16_t map[4096];

    int16_t beginOf2 = 0;
    int16_t endOf2 = 2047;

    int16_t beginOf4 = 2048;
    int16_t endOf4 = 3071;

    int16_t beginOf8 = 3072;
    int16_t endOf8 = 3583;

    int16_t beginOf16 = 3584;
    int16_t endOf16 = 4095;

    inline void init(){
        buffer = malloc(4096);
        connect(2, beginOf2, endOf2);
        connect(4, beginOf4, endOf4);
        connect(8, beginOf8, endOf8);
        connect(16, beginOf16, beginOf16);
    }

    inline void connect(int8_t size, int16_t begin, int16_t end){
        int idx = begin;
        while(idx < end){
            map[idx] = idx + size;
            idx += size;
        }
    }


}