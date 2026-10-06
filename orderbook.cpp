#include <iostream>
#include "orderbook.h"
#include <climits>
void OrderBook::clear() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    bids.clear();
    asks.clear();
    phonebook.clear();
    trade.clear();
}
void OrderBook::add_bid(const Order& order) {
    
  // Step 2: Copy the order (so we can modify it later)
    Order incoming = order;

    // Step 3: Choose the right map
    auto it = bids.find(incoming.price);
        if(it != bids.end()){
            //need to understand this more
            it->second.orders.push_back(incoming);
            auto list_it = std::prev(it->second.orders.end());
            it->second.total_quantity += incoming.quantity;
            phonebook[incoming.order_id] = list_it;
        }else{
            //dont wtf is even thise
            PriceLevel new_level{incoming.price, 0, {}};
            auto [map_it, inserted] = bids.emplace(incoming.price, new_level);
            map_it->second.orders.push_back(incoming);
            auto list_it = std::prev(map_it->second.orders.end());
            map_it->second.total_quantity += incoming.quantity;
            phonebook[incoming.order_id] = list_it;
        }
}

void OrderBook::add_ask(const Order& order){
    Order incoming = order;
      //checks if the price is alr there or not 
      auto it= asks.find(incoming.price);
      if(it != asks.end()){
          it->second.orders.push_back(incoming);
          auto list_it = std::prev(it->second.orders.end());
          it-> second.total_quantity += incoming.quantity;
          phonebook[incoming.order_id] = list_it;
      }
      else{
          //creates an new PriceLevel
          PriceLevel new_level{incoming.price, 0, {}};
          //inserting an new PriceLevel
          auto [map_it, inserted] = asks.emplace(incoming.price, new_level);
          map_it-> second.orders.push_back(incoming);
          auto list_it = std::prev(map_it-> second.orders.end());
          map_it-> second.total_quantity += incoming.quantity;
          phonebook[incoming.order_id] = list_it;
      }
}
bool OrderBook::can_fill_order(Order& order) const{
    int64_t remaining = order.quantity;
    if(order.side == Side::Buy){
        
        for (const auto& [price, level] : asks) {
            if(price > order.price)break;
            remaining -= level.total_quantity;
            if(remaining <=0)return true;
        }
        return false;
      }  
    else{
        for (const auto& [price, level] : bids) {

            if(price < order.price)break;
            remaining -= level.total_quantity;
            if(remaining <=0)return true;
            
        }
        return false;
      }   
}
void OrderBook::addOrder(const Order& order){
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    Order incoming = order;
    if(incoming.order_type == Ordertype::FOK){
          if(!can_fill_order(incoming)) return;
          else{
            if(incoming.side == Side::Buy){
                match_order(incoming);
            }else{
                sell_order(incoming);
            }
          }
    }

    else if(incoming.order_type ==Ordertype::Market || incoming.order_type == Ordertype::IOC){
        if(incoming.order_type ==Ordertype::Market) {
            if(incoming.side == Side::Buy){
                incoming.price = INT64_MAX;
                match_order(incoming);
            }else{
                incoming.price = 0;
                sell_order(incoming);
            }
        }else{
            if(incoming.side == Side::Buy){
                match_order(incoming);
            }else{
                sell_order(incoming);
            }
        }
    }else{
      if(incoming.side == Side::Buy){
          match_order(incoming);
          if(incoming.quantity > 0){
              add_bid(incoming);
          }
      }
      else{
         sell_order(incoming);
         if(incoming.quantity > 0){
            add_ask(incoming);
         }
      }
    }  
}

std::pair<int64_t,int64_t> OrderBook::best_ask() const{
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if(asks.empty()){
        return {0,0};
    }else{
        auto it = asks.begin();
        return {it ->first,it ->second.total_quantity};
    }
  //in the PriceLevel get the top pricelevel  
  //in the phonebook get the top element??
  //
}
std::pair<int64_t,int64_t> OrderBook::best_bid() const{
     std::lock_guard<std::recursive_mutex> lock(mtx_);
    if(bids.empty()){
        return {0,0};
    }else{
        auto it = bids.begin();
        return {it -> first,it ->second.total_quantity};
    }
}
// Cancel order
bool OrderBook::cancelOrder(uint64_t order_id){
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = phonebook.find(order_id);
    if(it == phonebook.end()){
        return false;
    }
    auto list_it = it-> second;
    Order& order_to_cancel = *list_it;
    if(order_to_cancel.side == Side::Buy){
          auto map_it = bids.find(order_to_cancel.price);
          if(map_it != bids.end()){
              map_it->second.orders.erase(list_it);
              map_it->second.total_quantity -= order_to_cancel.quantity;
              if(map_it->second.orders.empty()){
                bids.erase(map_it);
              }
          }
    }
    else{
        auto map_it = asks.find(order_to_cancel.price);
          if(map_it != asks.end()){
              map_it->second.orders.erase(list_it);
              map_it->second.total_quantity -= order_to_cancel.quantity;
              if(map_it->second.orders.empty()){
                  asks.erase(map_it);
              }
          }
    }
    phonebook.erase(it);
    return true;
}
bool OrderBook::modify_order(uint64_t order_id, int64_t new_price, int64_t new_quantity){
     std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = phonebook.find(order_id); 
    if(it == phonebook.end()){
        return false;
    }
    auto list_it = it->second;
    Order& order_to_modify = *list_it;
    if(order_to_modify.order_type == Ordertype::Market){
        return false;
    }
    uint64_t old_userid = order_to_modify.user_id;
    Side side = order_to_modify.side;
    cancelOrder(order_to_modify.order_id);
    Order new_order{order_id,old_userid,new_price,new_quantity,0,side,Ordertype::Limit};
    addOrder(new_order);
    return true;
}

//match order
void OrderBook::match_order(Order& order){
     //std::lock_guard<std::recursive_mutex> lock(mtx_);
    //Order incoming = order;
    while(order.quantity >0 && !asks.empty()){
      auto it = asks.begin();
      int64_t best_price = it-> first;
      int64_t best_quantity = it-> second.total_quantity;
      if(best_price > order.price){
          break;
      }
      auto& order_list = it->second.orders;
      auto& resting_order = order_list.front();
      int64_t fill_qty = std::min(order.quantity, resting_order.quantity);
        // Step 5: Reduce quantitie
      order.quantity -= fill_qty;
      resting_order.quantity -= fill_qty;
        // Step 6: Update total quantity at this price level
      it->second.total_quantity -= fill_qty;
      Trade t{best_price,fill_qty,order.order_id,resting_order.order_id,order.timestamp};
      trade.push_back(t);
        // Step 7: Emit trade (print for now)
      //std::cout << "TRADE: " << fill_qty << " @ " << best_price << std::endl;
        // Step 8: Remove resting order if fully filled
      if (resting_order.quantity == 0) {
            // Remove from phonebook
          phonebook.erase(resting_order.order_id);
            // Remove from list
          order_list.pop_front();
      }
        // Step 9: Remove price level if empty
      if (order_list.empty()) {
            asks.erase(it);
      }     
    }
}
//same as match_order but the differs is just the asks and sell is for the bids book. 
void OrderBook::sell_order(Order& order){
    //std::lock_guard<std::recursive_mutex> lock(mtx_); 
  // Order incoming = order;
    while(order.quantity >0 && !bids.empty()){
        auto it = bids.begin( );
        int64_t best_price = it-> first;
        int64_t best_quantity = it-> second.total_quantity;
        if(order.price> best_price){
            break;
        }
        auto& order_list = it->second.orders;
        auto& resting_order = order_list.front();
        int64_t fill_qty = std::min(order.quantity, resting_order.quantity);
        // Step 5: Reduce quantities
        order.quantity -= fill_qty;
        resting_order.quantity -= fill_qty;
        // Step 6: Update total quantity at this price level
        it->second.total_quantity -= fill_qty;
        Trade t{best_price,fill_qty,resting_order.order_id,order.order_id,order.timestamp};
        trade.push_back(t);
        // Step 7: Emit trade (print for now)
       // std::cout << "TRADE: " << fill_qty << " @ " << best_price << std::endl;
        // Step 8: Remove resting order if fully filled
        if (resting_order.quantity == 0) {
            // Remove from phonebook
            phonebook.erase(resting_order.order_id);
            // Remove from list
            order_list.pop_front();
        }
        // Step 9: Remove price level if empty
        if (order_list.empty()) {
            bids.erase(it);
        }     
    }
}
DepthSnapShot OrderBook::get_depth(size_t n)const {
     std::lock_guard<std::recursive_mutex> lock(mtx_);
    DepthSnapShot snap;
    for(auto it = bids.begin(); it != bids.end() && snap.bids.size() <n;++it){
        snap.bids.push_back(LevelInfo{it-> first,it -> second.total_quantity});
    }
    for(auto it = asks.begin(); it != asks.end() && snap.asks.size() <n;++it){
        snap.asks.push_back(LevelInfo{it-> first,it -> second.total_quantity});
    }
    return snap;
}
const std::vector<Trade>& OrderBook::get_trades() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return trade;
}
bool OrderBook::check_invariants() const{
     std::lock_guard<std::recursive_mutex> lock(mtx_);
    if(!bids.empty() && !asks.empty()){
        if(bids.begin() ->first >= asks.begin() -> first){
            std::cout << "Crossed book" << std::endl;
            return false;
        }
    }
    for (const auto& [price, level] : bids) {
        if (!check_level(level)) return false;
    }
    for (const auto& [price, level] : asks) {
        if (!check_level(level)) return false;
    }
    return true;
}
bool OrderBook::check_level(const PriceLevel& level) const {
     //std::lock_guard<std::recursive_mutex> lock(mtx_); 
  // 1. Level must have orders
    if (level.orders.empty()) {
        std::cout << "INVARIANT FAIL: empty level in map\n";
        return false;
    }
    // 2. Sum of order quantities must equal total_quantity
    int64_t sum = 0;
    for (const auto& order : level.orders) {
        // 3. No zero-quantity orders
        if (order.quantity <= 0) {
            std::cout << "INVARIANT FAIL: zero-qty order\n";
            return false;
        }
        sum += order.quantity;
    }
    if (sum != level.total_quantity) {
        std::cout << "INVARIANT FAIL: qty drift\n";
        return false;
    }
    return true;
}


