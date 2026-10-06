#include <map>
#include <iostream>
#include "poolallocator.h"

int main() {
    std::map<int, int, std::less<int>,
             PoolAllocator<std::pair<const int, int>>> m;
    for (int i = 0; i < 100; ++i) m[i] = i * 2;
    std::cout << "Map size: " << m.size() << "\n";
    std::cout << "m[50] = " << m[50] << "\n";
    return 0;
}
