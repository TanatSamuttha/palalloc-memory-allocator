#ifndef PALALLOC_WRAPPER_H
#define PALALLOC_WRAPPER_H

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

#include "palalloc.h"

template<class T>
struct PalallocWrapper {
    using value_type = T;

    template<class>
    friend struct PalallocWrapper;

    Palalloc* palalloc = nullptr;

    PalallocWrapper() noexcept = default;

    PalallocWrapper(Palalloc& p) noexcept : palalloc(&p) {}

    template<class U>
    constexpr PalallocWrapper(const PalallocWrapper<U>& other) noexcept : palalloc(other.palalloc) {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }

        if (auto p = static_cast<T*>(palalloc->alloc(n * sizeof(T))))
            return p;

        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t n) noexcept {
        palalloc->free(p, n * sizeof(T));
    }

    using is_always_equal = std::false_type;
};

#endif