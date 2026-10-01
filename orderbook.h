#pragma once
#include <cstdint>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include "order.h"
#include "pricelevel.h"
#include <utility>
#include "trader.h"
#include "poolallocator.h"
struct LevelInfo {
      int64_t price;
      int64_t quantity;
};
struct DepthSnapShot{
    std::vector<LevelInfo> bids; 
    std::vector<LevelInfo> asks;
};
class OrderBook {
private:
    // 1. Bids: Highest price first
    std::map<int64_t, PriceLevel, std::greater<int64_t>,
         PoolAllocator<std::pair<const int64_t, PriceLevel>>> bids;
    std::map<int64_t, PriceLevel, std::less<int64_t>,
         PoolAllocator<std::pair<const int64_t, PriceLevel>>> asks;    
    // 3. Phonebook: Order ID -> Exact location in the list
    std::unordered_map<uint64_t, std::list<Order, PoolAllocator<Order>>::iterator> phonebook;
    std::vector<Trade> trade;
    bool check_level(const PriceLevel& level) const;

public:
    // Task 3/4: Adding orders
    void add_bid(const Order& order);
    void add_ask(const Order& order);
    void addOrder(const Order& order);
    void match_order(Order& order);
    void sell_order(Order& order);

    
    // Task 7 (Later): Cancellation
    bool cancelOrder(uint64_t order_id);

    // ⬇️⬇️⬇️ TASK 5: ADD THESE TWO LINES ⬇️⬇️⬇️
    std::pair<int64_t, int64_t> best_bid() const;
    std::pair<int64_t, int64_t> best_ask () const;
    // ⬆️⬆️⬆️ TASK 5: ADD THESE TWO LINES ⬆️⬆️⬆️
    const std::vector<Trade>& get_trades() const;
    //
    //
    bool modify_order(uint64_t order_id, int64_t new_price, int64_t new_quantity);
    DepthSnapShot get_depth(size_t n) const;
    bool check_invariants() const;
    bool can_fill_order(Order& order) const;
   };
