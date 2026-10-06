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
    std::array<std::atomic<bool>, N> slot_ready_{};   // ← NEW
public:
    bool push(const T& item){
        size_t t = tail_.load();
        size_t next;
        do {
            next = (t + 1) % N;
            if (next == head_.load()) return false;   // full
        } while (!tail_.compare_exchange_weak(t, next));

        // We now exclusively own slot t
        slots_[t] = item;
        slot_ready_[t].store(true);                    // publish
        return true;
    }
    bool pop(T& out){
        size_t h = head_.load();
        if (h == tail_.load()) return false;           // empty

        // Wait for the producer to finish writing
        while (!slot_ready_[h].load()) {
            // spin
        }

        out = slots_[h];
        slot_ready_[h].store(false);                   // reset for reuse
        head_.store((h + 1) % N);
        return true;
    }

    size_t capacity() const {return N -1;}
};
