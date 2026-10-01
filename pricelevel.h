#pragma once

#include <cstdint>
#include <list>
#include "order.h"
#include "poolallocator.h"

struct PriceLevel{
    int64_t price;
    int64_t total_quantity;
    std::list<Order, PoolAllocator<Order>> orders;
};
