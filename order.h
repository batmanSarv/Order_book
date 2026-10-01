#pragma once
#include <cstdint>
enum class Side{Buy,Sell};
enum class Ordertype{Limit, Market,IOC,FOK};
struct Order{
    uint64_t order_id;
    uint64_t user_id;
    //string order_type;
    int64_t price;
    int64_t quantity;
    uint64_t timestamp;
    Side side;
    Ordertype order_type;

   };
