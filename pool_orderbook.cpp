#include <iostream>
#include <cstddef>

int main(){
    Pool<int> pool(5);
    int* a = pool.allocate();
    int* b = pool.allocate();
    int* c = pool.allocate();
    *a = 42; *b = 100; *c = 7;
    std::cout << "*a=" << *a << "  *b=" << *b << "  *c=" << *c << "\n";
    std::cout << "used_: " << pool.used() << "/" << pool.capacity() << "\n";
    // print values, print used_/capacity
    pool.deallocate(b);
    std::cout << "After free(b), used_: " << pool.used() << "\n";
    int* d = pool.allocate();
    std::cout << "d == b (reused slot): " << (d == b) << "\n";
    return 0;  
}
