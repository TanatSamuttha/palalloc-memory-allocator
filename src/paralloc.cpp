#include <cstdlib>
#include <cstdint>

namespace paralloc{

    void* buffer;

    uint16_t map[4096];
    
    /*
        size 2 bytes is located at index 0
        size 4 bytes is located a index 1
        size 8 bytes is located a index 2
        size 16 bytes is located a index 3

        hashed by count trail zero and decrease by 1
    */
    uint16_t begin[4];
    uint16_t end[4];

    inline void init(){
        buffer = std::malloc(4096);

        begin[0] = 0;
        end[0] = 2047;
        
        begin[1] = 2048;
        end[1] = 3071;

        begin[2] = 3072;
        end[2] = 3583;

        begin[3] = 3584;
        end[3] = 4095;

        connect(2);
        connect(4);
        connect(8);
        connect(16);
    }

    inline void connect(uint8_t size){
        int sizeIdx = __builtin_ctz(size) - 1;
        int idx = begin[sizeIdx];
        int endIdx = end[sizeIdx];
        while(idx < endIdx){
            map[idx] = idx + size;
            idx += size;
        }
    }
    
    void* paralloc(uint8_t size){
        int sizeIdx = __builtin_ctz(size) - 1;
        void* ptr = buffer + begin[sizeIdx];
        begin[sizeIdx] = map[begin[sizeIdx]];

        return ptr;
    }

    inline void* malloc(size_t size){
        int idx = __builtin_ctz(size) - 1;
        if(idx > 3){
            return std::malloc(size);
        }
        else{
            return paralloc(size);
        }
    }
}