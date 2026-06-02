#include "paralloc.h"

int main(){
    paralloc::init();

    int* ptr = paralloc::paralloc(sizeof(int));
    

    return 0;
}