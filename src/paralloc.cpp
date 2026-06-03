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
    uint16_t virgin[4] = {0, 2048, 3072, 3584};
    uint16_t tail[4] = {2047, 3071, 3583, 4095};

    const uint16_t INVALID = 0xFFFF;
}