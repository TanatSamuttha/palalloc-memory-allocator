# palalloc
## pool-adaptive-linking memory-allocator

Palalloc is thread local pool based memory allocator that design to allocate, free and reset in O(1) time complexity with free-list trade off by lower data size flexibility than std::malloc.

## How it work?
- Use LIFO(Last-In First-Out) allocate and free.
- If out of block for allocating size class, palalloc will combine 2 last blocks of smaller size class recuirsively otherwise it will split the last block of larger size class into a half recursively. If still out of available spaces or the size is bigger than declared maxSize, pallaoc call std::malloc for fallback.
- Reset method is only reset meta-data but hard-reset method is also free the memory pool.

## How to use
1. Add palalloc.h into your project folder and include it.
2. Create object in class of Palalloc and assign pages and maxsize into parameters (1 page size is 4096 bytes. Maxsize can't lower than 8 bytes and can't higher than (pages * 4096) / 2. And maxsize can only divisible by 2 but you don't have to worry because the program will automatically find the smallest size that still fit for your maxsize and divisible by 2).
