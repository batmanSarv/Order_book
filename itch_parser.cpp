#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "orderbook.h"
#include <unordered_map>
#pragma pack(push, 1)

struct AddOrderMsg {
    char     message_type;      // 'A'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];      // 48-bit nanoseconds
    uint64_t order_ref;
    char     buy_sell;          // 'B' or 'S'
    uint32_t shares;
    char     stock[8];          // 8-char ticker
    uint32_t price;             // fixed-point, 4 decimals
};

struct OrderExecutedMsg {
    char     message_type;      // 'E'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct OrderCancelMsg {
    char     message_type;      // 'X'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
    uint32_t cancelled_shares;
};

struct OrderDeleteMsg {
    char     message_type;      // 'D'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_ref;
};

struct SystemEventMsg {
    char     message_type;      // 'S'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    char     event_code;
};

#pragma pack(pop)

// ============================================================
// Size verification — compile-time checks
// ============================================================
static_assert(sizeof(AddOrderMsg)      == 36, "AddOrderMsg must be 36 bytes");
static_assert(sizeof(OrderExecutedMsg) == 31, "OrderExecutedMsg must be 31 bytes");
static_assert(sizeof(OrderCancelMsg)   == 23, "OrderCancelMsg must be 23 bytes");
static_assert(sizeof(OrderDeleteMsg)   == 19, "OrderDeleteMsg must be 19 bytes");
static_assert(sizeof(SystemEventMsg)   == 12, "SystemEventMsg must be 12 bytes");

// ============================================================
// Big-endian byte-swap helpers
// ============================================================
inline uint16_t bswap16(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t bswap32(uint32_t v) { return __builtin_bswap32(v); }
inline uint64_t bswap64(uint64_t v) { return __builtin_bswap64(v); }

inline uint64_t read_be48(const uint8_t* p) {
    return (static_cast<uint64_t>(p[0]) << 40)
         | (static_cast<uint64_t>(p[1]) << 32)
         | (static_cast<uint64_t>(p[2]) << 24)
         | (static_cast<uint64_t>(p[3]) << 16)
         | (static_cast<uint64_t>(p[4]) << 8)
         |  static_cast<uint64_t>(p[5]);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <itch_file>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open " << argv[1] << "\n";
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file\n";
        return 1;
    }

    std::cout << "File size: " << size << " bytes\n";

    std::unordered_map<uint16_t, OrderBook> books;
    std::unordered_map<uint16_t, std::string> locate_to_ticker;
    size_t offset = 0;
    uint64_t messages = 0;
    uint64_t counts[256] = {0};
    uint64_t adds = 0, cancels = 0, deletes = 0;

    while (offset + 2 < buffer.size()) {
        uint16_t len = (static_cast<uint16_t>(buffer[offset]) << 8)
                     |  static_cast<uint16_t>(buffer[offset + 1]);

        if (offset + 2 + len > buffer.size()) break;

        char type = static_cast<char>(buffer[offset + 2]);
        const uint8_t* body = buffer.data() + offset + 2;

        ++messages;
        ++counts[static_cast<uint8_t>(type)];

        switch (type){
          case 'R': {
              // Stock Directory — map stock_locate → ticker
              uint16_t locate = (static_cast<uint16_t>(body[1]) << 8) | body[2];
              std::string ticker(reinterpret_cast<const char*>(body + 11), 8);
              while (!ticker.empty() && ticker.back() == ' ') ticker.pop_back();
              locate_to_ticker[locate] = ticker;
              break;
          }
          case 'A': {
                const auto* m = reinterpret_cast<const AddOrderMsg*>(body);
                uint16_t locate = (static_cast<uint16_t>(body[1]) << 8) | body[2];

                Order o{
                    bswap64(m->order_ref),
                    1,
                    static_cast<int64_t>(bswap32(m->price)),
                    static_cast<int64_t>(bswap32(m->shares)),
                    read_be48(m->timestamp),
                    (m->buy_sell == 'B') ? Side::Buy : Side::Sell,
                    Ordertype::Limit
                };
                books[locate].addOrder(o);   // creates book lazily
                ++adds;
                break;
            }        
          case 'X': {
              const auto* m = reinterpret_cast<const OrderCancelMsg*>(body);
              uint16_t locate = (static_cast<uint16_t>(body[1]) << 8) | body[2];
              uint64_t ref = bswap64(m->order_ref);
              auto it = books.find(locate);
              if (it != books.end()) {
                  it->second.cancelOrder(ref);
              }
              ++cancels;
              break;
            }
          case 'D': {
                const auto* m = reinterpret_cast<const OrderDeleteMsg*>(body);
                uint16_t locate = (static_cast<uint16_t>(body[1]) << 8) | body[2];
                uint64_t ref = bswap64(m->order_ref);
                auto it = books.find(locate);
                if (it != books.end()) {
                    it->second.cancelOrder(ref);
                }
                ++deletes;
                break;
            }
          default:
                break;
        }

        offset += 2 + len;
    }

    std::cout << "Walked " << messages << " messages\n";
    std::cout << "Adds: " << adds << ", Cancels: " << cancels
              << ", Deletes: " << deletes << "\n";

     std::cout << "\nTop-of-book for " << books.size() << " instruments:\n";

    int shown = 0;
    bool all_ok = true;
    for (auto& [locate, book] : books) {
        if (!book.check_invariants()) {
            std::cerr << "INVARIANT FAIL on locate " << locate << "\n";
            all_ok = false;
        }
    }
    std::cout << books.size() << " books, "
              << (all_ok ? "all consistent" : "INVARIANTS VIOLATED") << "\n";
    for (auto& [locate, book] : books) {
        auto [bid, bid_qty] = book.best_bid();
        auto [ask, ask_qty] = book.best_ask();
        if (bid == 0 && ask == 0) continue;
        if (shown++ >= 10) break;   // print top 10

        std::string name = locate_to_ticker.count(locate)
                        ? locate_to_ticker[locate]
                        : "locate_" + std::to_string(locate);

        std::cout << "  " << name
                  << "  bid " << bid << " @ " << bid_qty
                  << "  |  ask " << ask << " @ " << ask_qty << "\n";
    }
    return 0;
}
