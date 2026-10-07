# Order Book Matching Engine

A C++17 limit order book matching engine with optional multi-threaded
operation via a lock-free MPSC queue. Supports limit, market, IOC and FOK
orders, multi-level price sweeps, a full trade log, and a custom pool
allocator for the book's map and list nodes.

**C++17** · **MIT License**

## Overview

The matching engine is the core of every exchange. This one keeps a
two-sided book (bids and asks), matches incoming orders by price-time
priority, and records every execution.

It is a ground-up implementation built to work through low-level systems
concerns: memory layout, iterator stability, allocator behavior, lock-free
queue design, and benchmarking methodology.

Two operating modes:

- **Single-threaded** (`OrderBook`): direct method calls. Fastest for one
  producer. Ideal for backtesting and simulation.
- **Multi-threaded** (`MatchingEngine`): producers push into a lock-free
  MPSC ring queue; a dedicated matcher thread consumes and processes.
  Scales across cores without mutex contention.

## Features

| Feature | Status |
|---------|--------|
| Limit orders (rest if unfilled) | ✅ |
| Market orders (fill at any price, discard remainder) | ✅ |
| IOC: Immediate-or-Cancel | ✅ |
| FOK: Fill-or-Kill (all-or-nothing) | ✅ |
| Multi-level price sweeps | ✅ |
| Cancellation by order ID (O(1) lookup + unlink) | ✅ |
| Order modification (cancel + re-add) | ✅ |
| Trade log (price, quantity, buy/sell IDs) | ✅ |
| Depth snapshot (top N levels) | ✅ |
| Invariant checker (test builds) | ✅ |
| Pool allocator for map and list nodes | ✅ |
| **Lock-free MPSC ring queue** | ✅ |
| **Dedicated matcher thread** | ✅ |

## Design

### Data structures

| Member | Type | Purpose |
|--------|------|---------|
| `bids` | `std::map<int64_t, PriceLevel, std::greater<>, PoolAllocator<...>>` | Buy side, best (highest) price first |
| `asks` | `std::map<int64_t, PriceLevel, std::less<>, PoolAllocator<...>>` | Sell side, best (lowest) price first |
| `phonebook` | `std::unordered_map<uint64_t, std::list<Order, PoolAllocator<Order>>::iterator>` | Order ID → list iterator for cancel/modify |
| `trades` | `std::vector<Trade>` | Append-only execution log |
| `queue_` (MatchingEngine) | `RingQueue<Order, 65536>` | Lock-free MPSC queue between producers and matcher |

### Key decisions

- **Integer prices.** Prices are `int64_t` ticks, avoiding floating-point rounding bugs.
- **FIFO within a level.** Orders at one price sit in a `std::list<Order>`, giving time priority.
- **Iterator stability.** `std::list` keeps other iterators valid after a mid-list erase. The phonebook depends on this.
- **Sentinel prices.** Market orders use `INT64_MAX` (buy) or `0` (sell), so one matching routine handles both market and limit orders.
- **FOK dry run.** Available liquidity is computed before the book is touched, so a rejected FOK leaves no trace.
- **Pool allocation.** `Pool<T>` and `PoolAllocator<T>` serve the map and list nodes from a pre-allocated free list. The phonebook and trade log still use the default allocator.
- **Lock-free queue.** `RingQueue<T, N>` uses CAS to claim slots and per-slot ready flags for publication. Multiple producers push without locking; one consumer drains.

### Complexity

- **Cancel:** O(1) lookup + unlink. If the level becomes empty, erasing it from the `std::map` is O(log n).
- **Match:** O(k) in the number of orders and levels consumed, plus O(log n) per level erased.
- **Push (MPSC queue):** O(1) amortized (CAS loop, one winner per slot).

### Matching algorithm

```
addOrder(order):
    FOK:          if !can_fill(order): reject, else match
    Market / IOC: match; never rests
    Limit:        match; rest any leftover in the book
```

The match loop sweeps the opposite side one price level at a time and stops when:

1. The incoming order is fully filled, or
2. The opposite side is empty, or
3. The next level no longer crosses the incoming price.

A trade is recorded on every iteration.

## Project Structure

```
orderbook/
├── order.h              # Order, Side, OrderType
├── pricelevel.h         # Orders resting at a single price
├── trade.h              # Trade record
├── pool.h               # Pool<T>: pre-allocated free list
├── poolallocator.h      # STL allocator interface over Pool<T>
├── orderbook.h          # Engine class declaration
├── orderbook.cpp        # Engine implementation
├── RingQueue.h          # Lock-free MPSC ring buffer
├── MatchingEngine.h     # Queue + matcher thread wrapper
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

g++ -std=c++17 -O3 -march=native -pthread \
    bench_orderbook.cpp orderbook.cpp \
    -lbenchmark -lbenchmark_main \
    -o bench

taskset -c 1 ./bench --benchmark_repetitions=50 --benchmark_report_aggregates_only=true
```

## Testing

33 hand-written scenarios across 8 sections, each followed by invariant checks.

| Section | Coverage |
|---------|----------|
| Basics | Add, cancel, best bid/ask, empty book |
| Matching | Full match, partial fill, no-cross, multi-level sweep |
| Market orders | Full fill, partial (discard), empty book |
| Trade log | Buy side, sell side, multi-trade IDs |
| Modify | Time-priority loss, price change, crossing modify, invalid |
| Depth snapshot | Top-N, truncation, over-request |
| IOC | Full fill, partial, price respect |
| FOK | Full fill, rejection, multi-level, price respect |

**Invariants** verified after every operation:

1. The book is never crossed (`best_bid < best_ask` when both sides non-empty).
2. Each level's `total_quantity` equals the sum of its order quantities.
3. No zero-quantity orders rest in the book.
4. Every level in the map holds a non-empty order list.

A violation prints a diagnostic and exits. The checker is not part of the benchmark build.

## Performance

### Benchmark environment

| Item | Value |
|------|-------|
| CPU | Intel Core i3-1115G4 (2 physical cores / 4 threads, base 3.00 GHz, boost 4.10 GHz) |
| Compiler | g++ (GCC) 16.2.1 |
| Kernel | 7.2.7-arch1-1 |
| Turbo / SMT | Turbo ON, SMT ON |
| Governor | performance |
| Pinning | `taskset -c 1` (single) / `taskset -c 0-1` (multi) |
| Build | `-O3 -march=native -pthread` |
| Method | Google Benchmark, 5–50 repetitions, median reported |

### Single-threaded

One iteration of each benchmark:
- **Insert** = one add of a non-crossing limit order into a pre-filled book
- **Cancel** = one cancel of an existing order
- **Match** = one market buy sweeping one ask level
- **Mixed** = one add + one cancel of the same order

| Benchmark | Median | CV | Throughput |
|-----------|--------|-----|------------|
| Insert | 139 ns | 1.6% | 7.2M ops/s |
| Cancel | 98 ns | 0.7% | 10.2M ops/s |
| Match | 108 ns | 0.9% | 9.3M ops/s |
| Mixed add + cancel | 94 ns | 1.7% | 10.7M ops/s |

### Multi-threaded: mutex (v3) vs lock-free queue (v5)

Wall time for 100K orders total across N producers.

| Producers | v3 (mutex) | v5 (MPSC queue) | Speedup |
|-----------|------------|------------------|---------|
| 1 | 8.6 ms | 2.5 ms | 3.4x |
| 2 | 61.7 ms | 7.2 ms | 8.6x |
| 4 | 225.7 ms | 15.1 ms | **15.0x** |

The mutex version collapses under contention (throughput drops as producers increase). The lock-free queue holds steady — the matcher thread is the bottleneck, not synchronization.

### At-scale (single-threaded)

| Book size | Insert | Cancel | Match |
|-----------|--------|--------|-------|
| 1K | 866 ns | 10 ns | 96 ns |
| 10K | 862 ns | 9 ns | 117 ns |
| 100K | 878 ns | 10 ns | 417 ns |
| 1M | 890 ns | 870 ns | 560 ns |
| 5M | 944 ns | 930 ns | 542 ns |

Three findings:

1. **Cancel is cache-bound at scale.** O(1) in algorithm, but at 1M+ entries every hashmap lookup is a cache miss (~900 ns). At 100K it's ~10 ns.
2. **Insert is O(log n) at every scale.** At 5M orders, the red-black tree is 22 levels deep. Every level is a cache miss. ~900 ns per insert.
3. **Match scales sub-linearly.** 96 ns at 1K → 542 ns at 5M — 5000x book growth for 5.6x latency growth.

### Optimization history

| Version | Change | Impact | Kind |
|---------|--------|--------|------|
| v1.0 | Baseline (`std::allocator`) | — | — |
| v2.0 | Pool allocator (map + list nodes) | -14% | Code |
| v2.0 | CPU governor locked | -25% | Environment |
| v3.0 | Mutex (thread-safe, no scaling) | +56% single-thread | Code |
| v4.0 | SPSC lock-free queue + matcher | (pipeline) | Code |
| v5.0 | MPSC queue (CAS + slot_ready) | **15x at 4 producers** | Code |

Numbers come from one machine and synthetic workloads. Use them as relative comparisons only. Tail latency (p99, p99.9) has not been measured.

## Limitations

- Single-instrument. No multi-asset support.
- No self-trade prevention.
- No partial cancel (modify is cancel + re-add).
- The phonebook and trade log still allocate from the default heap.
- `RingQueue` is fixed-capacity (65536). If producers outrun the matcher, `submit()` spins.
- Tests are hand-written cases only. No randomized differential test against a reference implementation. No ASan/UBSan runs.
- Latency is reported as medians only. No tail percentiles.

## License

MIT. See LICENSE.
