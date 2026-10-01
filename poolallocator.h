#pragma once
#include <cstddef>
#include <new>
#include "pool.h"

template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    PoolAllocator() noexcept { }

    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept { }

    static Pool<T>& get_pool() {
        static Pool<T> pool(1000000);
        return pool;
    }

    T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_alloc();
        T* p = get_pool().allocate();
        if (p == nullptr) throw std::bad_alloc();
        return p;
    }

    void deallocate(T* p, std::size_t) {
        get_pool().deallocate(p);
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>&) const noexcept { return true; }

    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }
};
