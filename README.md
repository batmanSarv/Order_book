# Order Book Matching Engine

A single-threaded limit order book matching engine in C++17.

Supports limit, market, IOC and FOK orders, multi-level price sweeps, a full trade log, and a custom pool allocator for the book's map and list nodes.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![License: MIT](https://img.shields.io/badge/license-MIT-green)

## Overview

The matching engine is the core of every exchange. This one keeps a two-sided book (bids and asks), matches incoming orders by price-time priority, and records every execution.

It is a ground-up implementation built to work through low-level systems concerns: memory layout, iterator stability, allocator behavior, and benchmarking methodology.

## Features

| Feature | Status |
|---|---|
| Limit orders (rest if unfilled) | ✅ |
| Market orders (fill at any price, discard remainder) | ✅ |
| IOC: Immediate-or-Cancel | ✅ |
| FOK: Fill-or-Kill (all-or-nothing) | ✅ |
| Multi-level price sweeps | ✅ |
| Cancellation by order ID (O(1) lookup and unlink, see note below) | ✅ |
| Order modification (cancel + re-add, loses time priority) | ✅ |
| Trade log (price, quantity, buy/sell IDs) | ✅ |
| Depth snapshot (top N levels) | ✅ |
| Invariant checker (test builds) | ✅ |
| Pool allocator for map and list nodes | ✅ |

## Design

### Data structures

| Member | Type | Purpose |
|---|---|---|
| `bids` | `std::map<int64_t, PriceLevel, std::greater<>, PoolAllocator<...>>` | Buy side, best (highest) price first |
| `asks` | `std::map<int64_t, PriceLevel, std::less<>, PoolAllocator<...>>` | Sell side, best (lowest) price first |
| `phonebook` | `std::unordered_map<uint64_t, std::list<Order, PoolAllocator<Order>>::iterator>` | Order ID → list iterator for cancel/modify |
| `trades` | `std::vector<Trade>` | Append-only execution log |

### Key decisions

- **Integer prices.** Prices are `int64_t` ticks, which avoids floating-point rounding bugs.
- **FIFO within a level.** Orders at one price sit in a `std::list<Order>`, giving time priority.
- **Iterator stability.** `std::list` keeps other iterators valid after a mid-list erase. The phonebook depends on this.
- **Sentinel prices.** Market orders use `INT64_MAX` (buy) or `0` (sell), so one matching routine handles both market and limit orders.
- **FOK dry run.** Available liquidity is computed before the book is touched, so a rejected FOK leaves no trace.
- **Pool allocation.** `Pool<T>` and `PoolAllocator<T>` serve the map and list nodes from a pre-allocated free list. The phonebook (`unordered_map`) and trade log (`vector`) still use the default allocator, so inserts still reach `malloc` through the phonebook.

### Complexity

- **Cancel:** looking up the order and unlinking it from its level is O(1). If the level becomes empty, erasing it from the `std::map` is O(log n) in the number of price levels.
- **Match:** O(k) in the number of orders and levels consumed, plus O(log n) per level erased.

### Matching algorithm

```
addOrder(order):
    FOK:          if !can_fill(order): reject, else match
    Market / IOC: match; never rests
    Limit:        match; rest any leftover in the book
```

The match loop sweeps the opposite side one price level at a time and stops when:

1. the incoming order is fully filled,
2. the opposite side is empty, or
3. the next level no longer crosses the incoming price.

A trade is recorded on every iteration.

## Project Structure

```
orderbook/
├── order.h              # Order, Side, OrderType
├── pricelevel.h         # Orders resting at a single price
├── trade.h              # Trade record
├── pool.h               # Pool<T>: pre-allocated free list
├── pool_allocator.h     # STL allocator interface over Pool<T>
├── orderbook.h          # Engine class declaration
├── orderbook.cpp        # Engine implementation
├── bench_orderbook.cpp  # Google Benchmark harness
├── main.cpp             # Test suite (33 tests, 8 sections)
├── LICENSE
└── README.md
```

## Build and Run

Requires g++ or clang++ with C++17 on Linux or macOS.

### Tests

```bash
g++ -std=c++17 orderbook.cpp main.cpp -o main
./main
```

### Benchmark

```bash
# Arch:          sudo pacman -S benchmark
# Debian/Ubuntu: sudo apt install libbenchmark-dev

g++ -std=c++17 -O3 -march=native \
    bench_orderbook.cpp orderbook.cpp \
    -lbenchmark -lbenchmark_main -lpthread \
    -o bench

taskset -c 1 ./bench --benchmark_repetitions=50 --benchmark_report_aggregates_only=true
```

## Testing

33 hand-written scenarios across 8 sections, each followed by invariant checks.

| Section | Coverage |
|---|---|
| Basics | Add, cancel, best bid/ask, empty book |
| Matching | Full match, partial fill, no-cross, multi-level sweep |
| Market orders | Full fill, partial (discard), empty book |
| Trade log | Buy side, sell side, multi-trade IDs |
| Modify | Time-priority loss, price change, crossing modify, invalid |
| Depth snapshot | Top-N, truncation, over-request |
| IOC | Full fill, partial, price respect |
| FOK | Full fill, rejection, multi-level, price respect |

### Invariants

After every operation in the test build, `check_invariants()` verifies:

1. The book is never crossed (`best_bid < best_ask` when both sides are non-empty).
2. Each level's `total_quantity` equals the sum of its order quantities.
3. No zero-quantity orders rest in the book.
4. Every level in the map holds a non-empty order list.

A violation prints a diagnostic and exits. The checker is not part of the benchmark build.

## Performance

**Environment**

| Item | Value |
|---|---|
| CPU | 2 physical cores / 4 threads, base 3.00 GHz, max turbo 4.10 GHz|
| Compiler | g++ (GCC) 16.2.1 20260810 |
| Kernel | 	7.2.7-arch1-1 |
| Turbo / SMT | Turbo ON, SMT ON (2 threads per core)|
| Governor | `performance` |
| Pinning | `taskset -c 1` |
| Build | `-O3 -march=native` |

Google Benchmark, 50 repetitions, median reported.

| Benchmark | Median | CV | Throughput |
|---|---|---|---|
| Insert | 109 ns | 9.1% | 9.2M ops/s |
| Cancel | 96 ns | 9.9% | 10.5M ops/s |
| Match | 111 ns | 15.2% | 9.1M ops/s |
| Mixed add + cancel | 60.4 ns | 3.1% | 16.6M ops/s |

One iteration of each benchmark is: <FILL: define per benchmark, e.g. Insert = one add of a non-crossing limit order; Mixed = one add followed by cancel of ..., and what "ns" is divided by>.

### Optimization history (mixed workload)

| Step | Median | Kind |
|---|---|---|
| Baseline (`std::allocator`) | 93.4 ns | n/a |
| Pool allocator on map nodes | 85.9 ns | Code |
| Pool allocator on list nodes | 80.2 ns | Code |
| CPU governor set to `performance` | 60.4 ns | Environment |

Total: 93.4 ns → 60.4 ns (-35%). Most of that gain (80.2 → 60.4 ns) came from the governor setting, not from code. The two allocator steps (-8%, -6%) are small next to the 9-15% run-to-run variation seen on the single-operation benchmarks, and per-step variation was not recorded, so treat them as indicative rather than proven.

Numbers come from one machine and one synthetic workload. Use them as relative comparisons only. Tail latency (p99, p99.9) has not been measured.

## Limitations

- Single-threaded. No locking or concurrency of any kind.
- No self-trade prevention.
- No partial cancel (modify is cancel + re-add).
- The phonebook and trade log still allocate from the default heap.
- Testing is hand-written cases only: no randomized differential test against a reference implementation, no sanitizer (ASan/UBSan) runs, and no dedicated tests for the pool allocator.
- Latency is reported as medians only.
## License

MIT. See [LICENSE](LICENSE).
