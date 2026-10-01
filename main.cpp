#include <iostream>
#include "orderbook.h"
#include <cstdlib>

// ---------- Helper ----------
void print_book_state(const OrderBook& book, const std::string& label = "") {
    if (!label.empty()) std::cout << "  " << label << std::endl;
    auto [bid_p, bid_q] = book.best_bid();
    auto [ask_p, ask_q] = book.best_ask();
    std::cout << "    Best Bid: " << bid_p << " @ " << bid_q << std::endl;
    std::cout << "    Best Ask: " << ask_p << " @ " << ask_q << std::endl;
}

void print_trades(const OrderBook& book, const std::string& label = "") {
    if (!label.empty()) std::cout << "  " << label << std::endl;
    const auto& trades = book.get_trades();
    std::cout << "    Trade count: " << trades.size() << std::endl;
    for (const auto& t : trades) {
        std::cout << "      price=" << t.price
                  << " qty=" << t.quantity
                  << " buy=" << t.buy_order_id
                  << " sell=" << t.sell_order_id << std::endl;
    }
}
void verify(const OrderBook& book, const std::string& label = "") {
    if (!book.check_invariants()) {
        std::cout << "!!! INVARIANT FAIL after: " << label << " !!!\n";
        std::exit(1);
    }
}

// ---------- Main ----------
int main() {
    std::cout << "=== ORDER BOOK TEST SUITE ===\n" << std::endl;

    // ============================================
    // SECTION 1: BASICS (add, cancel, best bid/ask)
    // ============================================
    std::cout << "--- SECTION 1: Basics ---" << std::endl;

    {
        OrderBook book;
        Order o1{1, 1001, 10000, 10, 0, Side::Buy, Ordertype::Limit};
        Order o2{2, 1002, 9900, 5, 0, Side::Buy, Ordertype::Limit};
        Order o3{3, 1003, 10000, 3, 0, Side::Buy, Ordertype::Limit};
        Order o4{4, 2001, 10100, 7, 0, Side::Sell, Ordertype::Limit};
        Order o5{5, 2002, 10200, 3, 0, Side::Sell, Ordertype::Limit};

        book.addOrder(o1); book.addOrder(o2); book.addOrder(o3);
        book.addOrder(o4); book.addOrder(o5);

        std::cout << "\n[Test 1] Book with 5 orders:" << std::endl;
        print_book_state(book);
        // Expected: Bid = 10000 @ 13, Ask = 10100 @ 7

        std::cout << "\n[Test 2] Cancel order #2 (not best bid):" << std::endl;
        bool r1 = book.cancelOrder(2);
        std::cout << "  Result: " << (r1 ? "SUCCESS" : "FAILED") << std::endl;
        print_book_state(book);
        // Expected: unchanged (order #2 was at 9900)

        std::cout << "\n[Test 3] Cancel order #4 (best ask):" << std::endl;
        bool r2 = book.cancelOrder(4);
        std::cout << "  Result: " << (r2 ? "SUCCESS" : "FAILED") << std::endl;
        print_book_state(book);
        // Expected: Ask = 10200 @ 3

        std::cout << "\n[Test 4] Cancel non-existent #99:" << std::endl;
        bool r3 = book.cancelOrder(99);
        std::cout << "  Result: " << (r3 ? "SUCCESS" : "FAILED") << std::endl;
        // Expected: FAILED
    }

    {
        std::cout << "\n[Test 5] Empty book:" << std::endl;
        OrderBook empty;
        print_book_state(empty);
        // Expected: 0 @ 0 for both
    }

    // ============================================
    // SECTION 2: MATCHING (Tasks 8-11)
    // ============================================
    std::cout << "\n--- SECTION 2: Matching ---" << std::endl;

    {
        std::cout << "\n[Test 6] Full match: Buy 5 @ $102 vs Ask 5 @ $101" << std::endl;
        OrderBook book;
        book.addOrder(Order{10, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{11, 1001, 10200, 5, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book);
        
        // Expected: TRADE 5 @ 10100, both 0 @ 0
    }

    {
        std::cout << "\n[Test 7] Partial match: Buy 10 vs Ask 5" << std::endl;
        OrderBook book;
        book.addOrder(Order{20, 2002, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{21, 1002, 10100, 10, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book);
        // Expected: TRADE 5 @ 10100, Bid = 10100 @ 5
    }

    {
        std::cout << "\n[Test 8] No match: Buy 5 @ $100 vs Ask 5 @ $101" << std::endl;
        OrderBook book;
        book.addOrder(Order{30, 2003, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{31, 1003, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book);
        // Expected: No trade. Bid = 10000 @ 5, Ask = 10100 @ 5
    }

    {
        std::cout << "\n[Test 9] Sell match: Sell 5 vs Bid 5 @ $100" << std::endl;
        OrderBook book;
        book.addOrder(Order{40, 1004, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{41, 2004, 10000, 5, 0, Side::Sell, Ordertype::Limit});
        print_book_state(book);
        // Expected: TRADE 5 @ 10000, both 0 @ 0
    }

    {
        std::cout << "\n[Test 10] Multi-level sweep: Buy 12 @ $103 through 3 ask levels" << std::endl;
        OrderBook book;
        book.addOrder(Order{50, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{51, 2002, 10200, 4, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{52, 2003, 10300, 6, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{53, 1001, 10300, 12, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book);
        verify(book, "Test 10 after 3 asks levels");
        // Expected: 3 trades (5@101, 4@102, 3@103), Ask = 10300 @ 3
    }

    // ============================================
    // SECTION 3: MARKET ORDERS (Tasks 12-13)
    // ============================================
    std::cout << "\n--- SECTION 3: Market Orders ---" << std::endl;

    {
        std::cout << "\n[Test 11] Market Buy, full fill" << std::endl;
        OrderBook book;
        book.addOrder(Order{60, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{61, 1001, 0, 5, 0, Side::Buy, Ordertype::Market});
        print_book_state(book);
        // Expected: TRADE 5 @ 10100, both 0 @ 0
    }

    {
        std::cout << "\n[Test 12] Market Buy, partial (discard leftover)" << std::endl;
        OrderBook book;
        book.addOrder(Order{62, 2002, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{63, 1002, 0, 10, 0, Side::Buy, Ordertype::Market});
        print_book_state(book);
        // Expected: TRADE 5 @ 10100, both 0 @ 0 (no leftover rests)
    }

    {
        std::cout << "\n[Test 13] Market Buy into empty book" << std::endl;
        OrderBook book;
        book.addOrder(Order{64, 1003, 0, 5, 0, Side::Buy, Ordertype::Market});
        print_book_state(book);
        // Expected: No trades, 0 @ 0
    }

    {
        std::cout << "\n[Test 14] Market Sell, full fill" << std::endl;
        OrderBook book;
        book.addOrder(Order{70, 1004, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{71, 2004, 0, 5, 0, Side::Sell, Ordertype::Market});
        print_book_state(book);
        // Expected: TRADE 5 @ 10000, both 0 @ 0
    }

    {
        std::cout << "\n[Test 15] Market Sell, partial (discard leftover)" << std::endl;
        OrderBook book;
        book.addOrder(Order{72, 1005, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{73, 2005, 0, 10, 0, Side::Sell, Ordertype::Market});
        print_book_state(book);
        // Expected: TRADE 5 @ 10000, both 0 @ 0
    }

    // ============================================
    // SECTION 4: TRADE LOG (Task 14)
    // ============================================
    std::cout << "\n--- SECTION 4: Trade Log ---" << std::endl;

    {
        std::cout << "\n[Test 16] Buy-side trade log" << std::endl;
        OrderBook book;
        book.addOrder(Order{80, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{81, 1001, 10100, 5, 0, Side::Buy, Ordertype::Limit});
        print_trades(book);
        // Expected: buy=81, sell=80
    }

    {
        std::cout << "\n[Test 17] Sell-side trade log (the flip)" << std::endl;
        OrderBook book;
        book.addOrder(Order{82, 1002, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{83, 2002, 10000, 5, 0, Side::Sell, Ordertype::Limit});
        print_trades(book);
        // Expected: buy=82 (resting), sell=83 (incoming)
    }

    {
        std::cout << "\n[Test 18] Multi-trade log (sweep)" << std::endl;
        OrderBook book;
        book.addOrder(Order{84, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{85, 2002, 10200, 4, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{86, 2003, 10300, 6, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{87, 1001, 10300, 12, 0, Side::Buy, Ordertype::Limit});
        print_trades(book);
        verify(book,"Test 18, Multi-trade log(sweep)");
        // Expected: 3 trades at 10100, 10200, 10300
    }

    // ============================================
    // SECTION 5: MODIFY ORDER (Task 15)
    // ============================================
    std::cout << "\n--- SECTION 5: Modify Order ---" << std::endl;

    {
        std::cout << "\n[Test 19] Modify quantity only (loses time priority)" << std::endl;
        OrderBook book;
        book.addOrder(Order{90, 1001, 10000, 10, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{91, 1002, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book, "Before modify:");
        // Best Bid = 10000 @ 15

        bool ok = book.modify_order(90, 10000, 8);
        std::cout << "  Modify #90 → qty 8: " << (ok ? "SUCCESS" : "FAILED") << std::endl;
        print_book_state(book, "After modify:");
        // Expected: Best Bid = 10000 @ 13 (8 + 5)
        // #90 has lost priority — it's now behind #91
    }

    {
        std::cout << "\n[Test 20] Modify to a new price" << std::endl;
        OrderBook book;
        book.addOrder(Order{92, 1001, 10000, 10, 0, Side::Buy, Ordertype::Limit});
        print_book_state(book, "Before modify:");
        // Best Bid = 10000 @ 10

        bool ok = book.modify_order(92, 9900, 10);
        std::cout << "  Modify #92 → price 9900: " << (ok ? "SUCCESS" : "FAILED") << std::endl;
        print_book_state(book, "After modify:");
        // Expected: Best Bid = 9900 @ 10
    }

    {
        std::cout << "\n[Test 21] Modify to a crossing price (should trade)" << std::endl;
        OrderBook book;
        book.addOrder(Order{93, 1001, 10000, 10, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{94, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        std::cout << "  Before: Bid = 10000, Ask = 10100" << std::endl;

        bool ok = book.modify_order(93, 10100, 10);
        std::cout << "  Modify #93 → price 10100: " << (ok ? "SUCCESS" : "FAILED") << std::endl;
        print_book_state(book, "After modify:");
        print_trades(book, "Trades from modify:");
        // Expected: TRADE 5 @ 10100, Bid leftover = 10100 @ 5
    }

    {
        std::cout << "\n[Test 22] Modify non-existent order" << std::endl;
        OrderBook book;
        bool ok = book.modify_order(999, 10000, 5);
        std::cout << "  Result: " << (ok ? "SUCCESS" : "FAILED") << std::endl;
        // Expected: FAILED
    }

    {
        std::cout << "\n[Test 23] Modify a Market order (should fail)" << std::endl;
        OrderBook book;
        book.addOrder(Order{95, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{96, 1001, 0, 5, 0, Side::Buy, Ordertype::Market});
        // #96 is fully filled — not in book anymore
        bool ok = book.modify_order(96, 10000, 5);
        std::cout << "  Result: " << (ok ? "SUCCESS" : "FAILED") << std::endl;
    }
    

    std::cout << "--- SECTION 6: Depth Snapshot---" << std::endl;


    {
        OrderBook book;
        book.addOrder(Order{1, 1001, 10000, 10, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{2, 1002, 9900, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{3, 1003, 9800, 8, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{4, 2001, 10100, 7, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{5, 2002, 10200, 3, 0, Side::Sell, Ordertype::Limit});

        auto depth = book.get_depth(3);
        std::cout << "Top 3 Bids:" << std::endl;
        for (const auto& l : depth.bids)
            std::cout << "  " << l.price << " @ " << l.quantity << std::endl;

        std::cout << "Top 3 Asks:" << std::endl;
        for (const auto& l : depth.asks)
            std::cout << "  " << l.price << " @ " << l.quantity << std::endl;

// Test truncation
        auto depth_small = book.get_depth(2);
        std::cout << "\nTop 2 Bids (should be 2 entries):" << std::endl;
        std::cout << "  Count: " << depth_small.bids.size() << std::endl;

// Test n > book size
        auto depth_big = book.get_depth(100);
        std::cout << "\nTop 100 Bids (book has 3, should return 3):" << std::endl;
        std::cout << "  Count: " << depth_big.bids.size() << std::endl;
    }

        // ============================================
    // SECTION 7: IOC + Market Sell (Task 19)
    // ============================================
    std::cout << "\n--- SECTION 7: IOC + Market Sell ---" << std::endl;

    {
        std::cout << "\n[Test 25] IOC Buy, full fill" << std::endl;
        OrderBook book;
        book.addOrder(Order{100, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 25 before IOC");

        book.addOrder(Order{101, 1001, 10100, 5, 0, Side::Buy, Ordertype::IOC});
        verify(book, "Test 25 after IOC");
        print_book_state(book);
        // Expected: TRADE 5 @ 10100, both 0 @ 0
    }

    {
        std::cout << "\n[Test 26] IOC Buy, partial (cancels leftover)" << std::endl;
        OrderBook book;
        book.addOrder(Order{102, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 26 before IOC");

        book.addOrder(Order{103, 1001, 10100, 10, 0, Side::Buy, Ordertype::IOC});
        verify(book, "Test 26 after IOC");
        print_book_state(book);
        // Expected: TRADE 5 @ 10100, NO leftover Bid (5 cancelled)
    }
    {
        std::cout << "\n[Test 27] IOC Buy respects price (decisive)" << std::endl;
        OrderBook book;
        book.addOrder(Order{104, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{105, 2002, 10200, 5, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 27 before IOC");

        book.addOrder(Order{106, 1001, 10100, 10, 0, Side::Buy, Ordertype::IOC});
        verify(book, "Test 27 after IOC");
        print_book_state(book);
        print_trades(book);
        // Expected: TRADE 5 @ 10100 ONLY
        //           Best Ask = 10200 @ 5 (untouched)
        //           No leftover Bid
    }
    {
        std::cout << "\n[Test 28] Market Sell, full fill" << std::endl;
        OrderBook book;
        book.addOrder(Order{110, 1001, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        verify(book, "Test 28 before Market Sell");

        book.addOrder(Order{111, 2001, 0, 5, 0, Side::Sell, Ordertype::Market});
        verify(book, "Test 28 after Market Sell");
        print_book_state(book);
        // Expected: TRADE 5 @ 10000, both 0 @ 0
    }
    {
        std::cout << "\n[Test 29] IOC Sell respects price (decisive)" << std::endl;
        OrderBook book;
        book.addOrder(Order{112, 1001, 10000, 5, 0, Side::Buy, Ordertype::Limit});
        book.addOrder(Order{113, 1002, 9900, 5, 0, Side::Buy, Ordertype::Limit});
        verify(book, "Test 29 before IOC Sell");

        book.addOrder(Order{114, 2001, 10000, 10, 0, Side::Sell, Ordertype::IOC});
        verify(book, "Test 29 after IOC Sell");
        print_book_state(book);
        print_trades(book);
        // Expected: TRADE 5 @ 10000 ONLY
        //           Best Bid = 9900 @ 5 (untouched)
        //           No leftover Ask
    }
        // ============================================
    // SECTION 8: FOK — Fill or Kill (Task 20)
    // ============================================
    std::cout << "\n--- SECTION 8: FOK ---" << std::endl;
    {
        std::cout << "\n[Test 30] FOK Buy, full fill" << std::endl;
        OrderBook book;
        book.addOrder(Order{120, 2001, 10100, 10, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 30 before FOK");

        book.addOrder(Order{121, 1001, 10100, 10, 0, Side::Buy, Ordertype::FOK});
        verify(book, "Test 30 after FOK");
        print_book_state(book);
        print_trades(book);
        // Expected: TRADE 10 @ 10100, both 0 @ 0
    }
    {
        std::cout << "\n[Test 31] FOK Buy rejected — not enough liquidity" << std::endl;
        OrderBook book;
        book.addOrder(Order{122, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 31 before FOK");

        book.addOrder(Order{123, 1001, 10100, 10, 0, Side::Buy, Ordertype::FOK});
        verify(book, "Test 31 after FOK");
        print_book_state(book);
        print_trades(book);
        // Expected: NO trades. Book UNCHANGED: Ask = 10100 @ 5
    }
    {
        std::cout << "\n[Test 32] FOK Buy sweeps multiple levels" << std::endl;
        OrderBook book;
        book.addOrder(Order{124, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{125, 2002, 10200, 5, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 32 before FOK");

        book.addOrder(Order{126, 1001, 10200, 10, 0, Side::Buy, Ordertype::FOK});
        verify(book, "Test 32 after FOK");
        print_book_state(book);
        print_trades(book);
        // Expected: TRADE 5 @ 10100, TRADE 5 @ 10200, book empty
    }
    {
        std::cout << "\n[Test 33] FOK Buy rejected — price stops crossing (decisive)" << std::endl;
        OrderBook book;
        book.addOrder(Order{127, 2001, 10100, 5, 0, Side::Sell, Ordertype::Limit});
        book.addOrder(Order{128, 2002, 10200, 10, 0, Side::Sell, Ordertype::Limit});
        verify(book, "Test 33 before FOK");

        book.addOrder(Order{129, 1001, 10100, 10, 0, Side::Buy, Ordertype::FOK});
        verify(book, "Test 33 after FOK");
        print_book_state(book);
        print_trades(book);
        // Expected: NO trades. Book UNCHANGED: Ask 5 @ 10100, Ask 10 @ 10200
        // The $10200 ask does NOT cross the $10100 FOK buy price.
    }
    std::cout << "sizeof(Order): " << sizeof(Order) << "\n";
    std::cout << "sizeof(PriceLevel): " << sizeof(PriceLevel) << "\n";
    std::cout << "\n=== ALL TESTS COMPLETE ===" << std::endl;
    return 0;
}
