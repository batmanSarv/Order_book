#include <iostream>
#include <thread>
#include <chrono>
#include "MatchingEngine.h"

int main() {
    MatchingEngine engine;

    // Push 5 sell orders at different prices
    for (int i = 0; i < 5; ++i) {
        Order o{
            static_cast<uint64_t>(i + 1),   // order_id
            1,                               // user_id
            10100 + i * 10,                  // price: 10100, 10110, 10120, ...
            10,                              // quantity
            0,                               // timestamp
            Side::Sell,
            Ordertype::Limit
        };
        engine.submit(o);
    }

    // Wait for matcher to drain the queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Read the book (safe — matcher is idle, no queue activity)
    auto [best_ask_price, best_ask_qty] = engine.book().best_ask();
    std::cout << "Best Ask: " << best_ask_price << " @ " << best_ask_qty << "\n";
    // Expected: 10100 @ 10

    auto [best_bid_price, best_bid_qty] = engine.book().best_bid();
    std::cout << "Best Bid: " << best_bid_price << " @ " << best_bid_qty << "\n";
    // Expected: 0 @ 0

    // Push one matching buy — should trade against best ask
    Order buy{
        100, 2,
        10100, 5, 0,
        Side::Buy,
        Ordertype::Limit
    };
    engine.submit(buy);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto [new_ask_price, new_ask_qty] = engine.book().best_ask();
    std::cout << "After buy — Best Ask: " << new_ask_price << " @ " << new_ask_qty << "\n";
    // Expected: 10100 @ 5 (order 1 reduced from 10 to 5)

    const auto& trades = engine.book().get_trades();
    std::cout << "Trades: " << trades.size() << "\n";
    // Expected: 1

    return 0;
}
