#pragma once

#include <atomic>
#include <thread>
#include "orderbook.h"
#include "RingQueue.h"
class MatchingEngine {
private:
    OrderBook book_;
    RingQueue<Order, 65536> queue_;
    std::thread matcher_;
    std::atomic<bool> stop_{false};
    
    void matcher_loop(){
        // your implementation
       Order o;
        while (!stop_.load()) {
            if (queue_.pop(o)) {
                book_.addOrder(o);
            }
        }
        // drain what's left
        while (queue_.pop(o)) {
            book_.addOrder(o);
        }
    }
public:
    MatchingEngine() {
        matcher_ = std::thread([this]() { matcher_loop(); });  
    }
    
    ~MatchingEngine() {
        stop_.store(true);
        matcher_.join();
    }    
    void submit(const Order& o) {
        while(!queue_.push(o)) { }
    }
    
    const OrderBook& book() const {
        return book_;
    }
};
