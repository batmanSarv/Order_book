#pragma once 

#include <array>
#include <atomic> 
#include <cstddef>

template <typename T, size_t N> 
class RingQueue{
private:
    static_assert(N >=2, "Capacity must be at least 2");

    std::array<T,N> slots_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    bool push(const T& item){
        size_t t = tail_.load();
        size_t next = (t+1) % N;
        if (next == head_.load()) return false;
        slots_[t] = item;
        tail_.store(next);
        return true;
    }
    bool pop(T& out){
        size_t h = head_.load();
        if (h == tail_.load()) return false;
        out = slots_[h];
        head_.store((h+1) % N);
        return true;
    }

    size_t capacity() const {return N -1;}
};
