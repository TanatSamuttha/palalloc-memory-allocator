#include "paralloc.h"

#include <cstdlib>

namespace paralloc{
    uint8_t* buffer;

    /*
        size 8 bytes is located at index 0
        size 16 bytes is located a index 1
        size 32 bytes is located a index 2
        size 64 bytes is located a index 3

        hashed by count trail zero and decrease by 1
    */
    uint16_t head[4] = {0, 2048, 3072, 3584};
    uint16_t bytesleft[4] = {2048, 1024, 512, 512};

    uint16_t INVALID = 0xFFFF;
}