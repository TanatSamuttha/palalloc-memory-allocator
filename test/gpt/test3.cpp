#include <iostream>
#include <vector>
#include <random>

#include "paralloc.h"

struct A{
    char data[8];
};

struct B{
    char data[16];
};

struct C{
    char data[32];
};

struct D{
    char data[64];
};

struct Node{
    void* ptr;
    int type;
};

int main(){

    std::cout << "Program start\n";

    Paralloc alloc;

    std::cout << "Allocator created\n";

    std::mt19937 rng(12345);

    std::vector<Node> alive;

    constexpr int OPS = 1000000;

    for(int step = 0; step < OPS; step++){

        // std::cout << "\n====================\n";
        // std::cout << "STEP = " << step << '\n';
        // std::cout << "alive = " << alive.size() << '\n';

        bool doAlloc =
            alive.empty() ||
            (rng() & 1);

        if(doAlloc){

            int type = rng() % 4;

            // std::cout << "ALLOC TYPE = " << type << '\n';

            Node node;
            node.type = type;

            // std::cout << "Before alloc\n";

            switch(type){

                case 0:
                    node.ptr = alloc.alloc<A>();
                    break;

                case 1:
                    node.ptr = alloc.alloc<B>();
                    break;

                case 2:
                    node.ptr = alloc.alloc<C>();
                    break;

                case 3:
                    node.ptr = alloc.alloc<D>();
                    break;
            }

            // std::cout << "After alloc\n";

            // std::cout
            //     << "PTR = "
            //     << node.ptr
            //     << '\n';

            // if(node.ptr == nullptr){

            //     std::cout
            //         << "NULL POINTER\n";

            //     return 1;
            // }

            // std::cout
            //     << "Writing test pattern\n";

            ((char*)node.ptr)[0] = 123;

            // std::cout
            //     << "Write success\n";

            alive.push_back(node);

            // std::cout
            //     << "Push vector success\n";
        }
        else{

            size_t idx =
                rng() % alive.size();

            Node node =
                alive[idx];

            // std::cout
            //     << "FREE INDEX = "
            //     << idx
            //     << '\n';

            // std::cout
            //     << "PTR = "
            //     << node.ptr
            //     << '\n';

            // std::cout
            //     << "TYPE = "
            //     << node.type
            //     << '\n';

            switch(node.type){

                case 0:
                    alloc.free((A*)node.ptr);
                    break;

                case 1:
                    alloc.free((B*)node.ptr);
                    break;

                case 2:
                    alloc.free((C*)node.ptr);
                    break;

                case 3:
                    alloc.free((D*)node.ptr);
                    break;
            }

            // std::cout
            //     << "Free success\n";

            alive[idx] =
                alive.back();

            alive.pop_back();

            // std::cout
            //     << "Vector update success\n";
        }
    }

    std::cout
        << "\nPASS\n";

    return 0;
}