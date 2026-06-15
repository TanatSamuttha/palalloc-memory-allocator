# pool-adaptive-linking memory-allocator (palalloc)

**Palalloc** is a thread-local, pool-based memory allocator designed for deterministic **O(1)** allocation, deallocation and reset operations. It trade of the size flexibility `std::malloc` for ultra-fast performance using an adaptive free-list mechanism.

## Feature
- Allocates and frees blocks in Last-In, First-Out (LIFO) order to maximize CPU cache locality by reusing the most recently vacated memory addresses.
- Initialy, Palalloc only indexes pool state (heads, tails, and virgin addresses) without pre-linking every block, reducing startup overhead.
- Allocating has 1 fast path and 4 slow paths.
   - The fast path imediately returns the head of the free-list.
   - The 1st slow path load a new chunk from the pool by linking 16 virgin blocks and return the first block.
   - The 2nd slow path combines last 2 blocks of smaller size class recursively.
   - The 3rd slow path splits last block of bigger size class in half recursively, then gives the front block as the new head of the smaller size class and returns the back block to the user.
   - The 4th slow path will call std::malloc when palalloc is out of pool space or the allocated size is bigger than the declared max size.
- Hard reset method will free the pool and reset heads, tails and virgins addresses.
- If allocate unsupported type, palalloc will allocate the block of smallest size class that bigger than allocating size.

## Benchmarks
This is results of speed comparing between palalloc and std::malloc. Execute the same .exe file 10 times
| Benchmarks | Description                                                   | Average times | Max times | Min times |
|:-----------|:--------------------------------------------------------------|:--------------|:----------|:----------|
| Game loop  | Simulating game loop. Allocate many times then reset the pool | 18.07x        | 19.48x    | 16.75x    |
| Chaotic    | Allocate and deallocate chaotically                           | 5.57x         | 8.02x     | 3.54x     |
| Split      | Stress test force palalloc to manage it's pool via split      | 2.74x         | 2.97x     | 2.52x     |

## How to use
1. Add palalloc.h into your project folder and include it.
2. Create object in class of Palalloc and assign pages and maxsize into parameters (1 page size is 4096 bytes. Maxsize can't lower than 8 bytes and can't higher than (pages * 4096) / 2. And maxsize can only divisible by 2 but you don't have to worry because the program will automatically find the smallest size that still fit for your maxsize and divisible by 2).

## API
| Method            | Parameters                            | Description                                                                                      |
|:------------------|:------------------------------------- |:-------------------------------------------------------------------------------------------------|
| Palalloc          | `void (size_t pages, size_t maxSize)` | Constructor. Assign pages and max size.                                                          |
| init              | `void ()`                             | Reserve pool memory and reset boundary index.                                                    |
| alloc             | `type* <type>()`                      | Allocates memory in the pool. Will return `nullptr` if allocating size is bigger than max size or pool overflow.                                                                                                                                                 |
| galloc            | `type* <type>()`                      | Stand for general-allocates. Allocates memory in the pool. Will fallback to `std::malloc` if allocating size is bigger than max size or pool overflow.                                                                                                      |
| free              | `void <type>(uint8_t* ptr)`           | Deallocates memory.                                                                              |
| reset             | `void ()`                             | Resets the pool state. **Warning:** All previously allocated pointers will become dangling. You must manually nullify them. This method does **not** free or track memory allocated via `std::malloc`.                                                              |
| hardReset         | `void ()`                             | Resets the pool state and free the pool. **Warning:** All previously allocated pointers will become dangling. You must manually nullify them. This method does **not** free or track memory allocated via `std::malloc`.                                           |
| calculateMinPages | `size_t (size_t maxSize)`             | A utility method to calculate the minimum number of pages required for a given `maxSize`.        |
| getPool           | `void* ()`                            | Beginning address of the pool.                                                                   |
| getHead           | `size_t <type>()`                     | Distance from pool begin to the head of free list.                                               |
| getTail           | `size_t <type>()`                     | Distance from pool begin to the tail of free list.                                               |
| getVirgin         | `size_t <type>()`                     | Distance from pool begin to the virgin of free list.                                             |
