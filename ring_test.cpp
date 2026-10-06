#include <iostream>
#include "RingQueue.h"

int main() {
    RingQueue<int, 8> q;   // 7 usable slots

    // Test 1: empty pop
    int x;
    std::cout << "Empty pop returns: " << q.pop(x) << " (expect 0)\n";

    // Test 2: FIFO order
    q.push(10);
    q.push(20);
    q.push(30);
    q.pop(x); std::cout << "Pop 1: " << x << " (expect 10)\n";
    q.pop(x); std::cout << "Pop 2: " << x << " (expect 20)\n";
    q.pop(x); std::cout << "Pop 3: " << x << " (expect 30)\n";

    // Test 3: fill to capacity (7 slots)
    for (int i = 0; i < 7; ++i) q.push(i * 100);
    std::cout << "Full push returns: " << q.push(999) << " (expect 0)\n";

    // Test 4: drain and verify
    std::cout << "Drain: ";
    while (q.pop(x)) std::cout << x << " ";
    std::cout << "(expect 0 100 200 300 400 500 600)\n";

    // Test 5: wrap-around (many push/pop cycles)
    int total = 0;
    for (int i = 0; i < 1000; ++i) {
        q.push(i);
        q.pop(x);
        total += x;
    }
    std::cout << "Wrap-around total: " << total
              << " (expect 499500 = sum 0..999)\n";

    return 0;
}
