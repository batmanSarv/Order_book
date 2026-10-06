#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "RingQueue.h"

int main() {
    RingQueue<int, 1024> q;
    const int num_producers = 4;
    const int per_producer = 10000;
    std::atomic<uint64_t> pushed{0};

    std::vector<std::thread> producers;
    for (int t = 0; t < num_producers; ++t) {
        producers.emplace_back([&]() {
            for (int i = 0; i < per_producer; ++i) {
                while (!q.push(i)) { }
                pushed.fetch_add(1);
            }
        });
    }

    int64_t popped = 0;
    int64_t target = num_producers * per_producer;
    while (popped < target) {
        int x;
        if (q.pop(x)) ++popped;
    }

    for (auto& p : producers) p.join();

    std::cout << "Target: " << target << "\n";
    std::cout << "Pushed: " << pushed.load() << "\n";
    std::cout << "Popped: " << popped << "\n";
    std::cout << "Match: " << ((popped == target) ? "YES" : "NO") << "\n";
    return 0;
}
