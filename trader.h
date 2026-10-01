#pragma once 
#include <cstdint>
struct Trade{
  int64_t price;
  int64_t quantity;
  uint64_t buy_order_id;
  uint64_t sell_order_id;
  uint64_t timestamp;
};
