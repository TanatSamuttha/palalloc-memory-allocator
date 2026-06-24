# pool-adaptive-linking memory-allocator (palalloc)

**Palalloc** is a thread-local, pool-based memory allocator designed for deterministic **O(1)** allocation, deallocation and reset operations. It trade of the size flexibility `std::malloc` for ultra-fast performance using an adaptive free-list mechanism.

## Implementation Details
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

| Benchmarks   | Description                                                                          | Average times | Max times | Min times |
|:-------------|:-------------------------------------------------------------------------------------|:--------------|:----------|:----------|
| Reset        | Intensive allocation followed by a full pool reset.                                  | 10.24x        | 12.13x    | 8.43x     |
| Random       | Randomized allocation and deallocation patterns.                                     | 3.66x         | 4.07x     | 3.18x     |
| Random Tiny  | Randomized allocation and deallocation patterns in small objects.                    | 5.40x         | 6.26x     | 3.89x     |
| FIFO         | Use first in first out strategy to test in CPU cache miss scenario                   | 5.42x         | 6.39x     | 4.47x     |
| Stress       | Stress test force palalloc to manage its pool via split and `std::malloc` fallback.  | 2.64x         | 3.20x     | 2.26x     |
| Swiss Cheese | Allocate many objects then random deallocate to make fragmentation then migrate **palalloc** pool to fix fragmentation       | 2.41x         | 2.63x     | 1.94x     |

Performance Analysis Note: In the 'Stress' scenario, the performance gain is lower compared to other tests. This is because the workload intentionally exceeds pool capacity, forcing the allocator to delegate tasks to `std::malloc`. This result demonstrates that Palalloc remains faster than standard allocation even when burdened with fallback overhead.

## How to use
Add palalloc.h into your project folder and include it in your source code.
```cpp
#include "palalloc.h"
```

## API Reference
| Method            | Parameters                            | Description                                                                                      |
|:------------------|:------------------------------------- |:-------------------------------------------------------------------------------------------------|
| Palalloc          | `void (size_t pages, size_t maxSize)` | Constructor. Assigns total pages and maximum allocate size limit.                                |
| init              | `void ()`                             | Reserve pool memory and resets indexes pool state.                                               |
| alloc             | `type* <type>()`                      | Allocates memory in the pool. Will return `nullptr` if the requested size is larger than the max size or if the pool overflows.                                                                                                                                 |
| galloc            | `type* <type>()`                      | Stand for general-allocate. Allocates memory in the pool. Fallback to `std::malloc` if the requested size is larger than the max size or if the pool overflows.                                                                                                     |
| free              | `void <type>(uint8_t* ptr)`           | Deallocates memory.                                                                              |
| reset             | `void ()`                             | Resets the pool state. **Warning:** All previously allocated pointers will become dangling. You must manually nullify them. This method does **not** free or track memory allocated via `std::malloc`.                                                              |
| hardReset         | `void ()`                             | Resets the pool state and free the pool. **Warning:** All previously allocated pointers will become dangling. You must manually nullify them. This method does **not** free or track memory allocated via `std::malloc`.                                           |
| calculateMinPages | `size_t (size_t maxSize)`             | A utility method to calculate the minimum number of pages required for a given `maxSize`.        |
| getPool           | `void* ()`                            | Beginning address of the pool.                                                                   |
| getHead           | `size_t <type>()`                     | Distance from pool begin to the head of the free list.                                           |
| getTail           | `size_t <type>()`                     | Distance from pool begin to the tail of the free list.                                           |
| getVirgin         | `size_t <type>()`                     | Distance from pool begin to the virgin of the virgin address.                                    |
