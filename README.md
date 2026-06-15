# palalloc
## pool-adaptive-linking memory-allocator

**Palalloc** is a thread-local, pool-based memory allocator designed for deterministic **O(1)** allocation, deallocation and reset operations. It trade of the size flexibility `std::malloc` for ultra-fast performance using an adaptive free-list mechanism.

## Feature
- Allocates and frees blocks in Last-In, First-Out (LIFO) order to maximize CPU cache locality by reusing the most recently vacated memory addresses.
- At initial, Palalloc only indexes the boundaries (heads, tails, and virgin addresses) without pre-linking every block, reducing startup overhead
- Allocating have 1 fast path and 4 slow paths.
   - The fast path imediately return the head of freelist.
   - 1st slow path load a new chunk from the pool by linking 16 virgin blocks and return the first block.
   - 2nd slow path combine last 2 blocks of smaller size class recursively.
   - 3rd slow path split last block of bigger size class into half recursively then give the front block to be new head of smaller size class and return the back block to user.
   - 4th slow path will call std::malloc when palalloc is out of pool space or the allocating size is bigger than declared max size.
- Hard reset method will free the pool and reset heads, tails and virgins addresses.
- If allocate unsupported type, palalloc will allocate the block of smallest size class that bigger than allocating size.

## Benchmarks
This is results of speed comparing between palalloc and std::malloc. Execute the same .exe file 10 times
| Benchmarks | Description | Average times | Max times | Min times |
|:-----------|:------------|:--------------|:----------|:----------|
| Game loop  |             | 18.07x        | 19.48x    | 16.75x    |
| Chaotic    |             | 5.57x         | 8.02x     | 3.54x     |
| Split      |             | 2.74x         | 2.97x     | 2.52x     |

## How to use
1. Add palalloc.h into your project folder and include it.
2. Create object in class of Palalloc and assign pages and maxsize into parameters (1 page size is 4096 bytes. Maxsize can't lower than 8 bytes and can't higher than (pages * 4096) / 2. And maxsize can only divisible by 2 but you don't have to worry because the program will automatically find the smallest size that still fit for your maxsize and divisible by 2).

## API
| Method   | Parameters                          | Description |
|:---------|:----------------------------------- |:------------|
| Palalloc | `void (size_t pages, size_t maxSize)` |
