# Order Book Matching Engine

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20windows-lightgrey.svg)

A C++17 limit order book matching engine with a lock-free MPSC queue, a dedicated matcher thread, and a NASDAQ ITCH 5.0 binary feed parser.

Supports limit, market, IOC, and FOK orders; multi-level price sweeps; a trade log; and a custom pool allocator for the book's map and list nodes.

---

## Overview

A ground-up matching engine built to work through low-level systems concerns: memory layout, iterator stability, allocator behavior, lock-free queue design, and benchmarking methodology.

Three operating modes:

1. **Single-threaded (`OrderBook`)**: direct method calls.
2. **Multi-threaded (`MatchingEngine`)**: producers push into a lock-free MPSC ring queue; one dedicated matcher thread consumes and is the only thread that mutates the book.
3. **ITCH replay (`itch_parser`)**: reads NASDAQ ITCH 5.0 binary feeds and routes Add / Cancel / Delete messages into per-instrument books.

## Features

| Feature | Status |
| :--- | :---: |
| Limit orders (rest if unfilled) | ✅ |
| Market orders (fill at any price, discard remainder) | ✅ |
| IOC: Immediate-or-Cancel | ✅ |
| FOK: Fill-or-Kill (all-or-nothing) | ✅ |
| Multi-level price sweeps, strict FIFO within a level | ✅ |
| Cancellation by order ID (O(1) lookup + unlink) | ✅ |
| Order modification (cancel + re-add) | ✅ |
| Trade log (price, quantity, buy/sell IDs) | ✅ |
| Depth snapshot (top N levels) | ✅ |
| Invariant checker (test builds) | ✅ |
| Pool allocator for map and list nodes | ✅ |
| Lock-free MPSC ring queue | ✅ |
| Dedicated matcher thread | ✅ |
| NASDAQ ITCH 5.0 parser | ✅ |

## Design

### Data structures

| Member | Type | Purpose |
| :--- | :--- | :--- |
| `bids` | `std::map<int64_t, PriceLevel, std::greater<>, PoolAllocator<...>>` | Buy side, best (highest) price first |
| `asks` | `std::map<int64_t, PriceLevel, std::less<>, PoolAllocator<...>>` | Sell side, best (lowest) price first |
| `phonebook` | `std::unordered_map<uint64_t, std::list<Order, PoolAllocator<Order>>::iterator>` | Order ID → list iterator for cancel/modify |
| `trade` | `std::vector<Trade>` | Append-only execution log |
| `queue_` | `RingQueue<Order, 65536>` | Lock-free MPSC queue between producers and matcher |

### Key decisions

- **Integer prices.** `int64_t` ticks, avoiding floating-point rounding bugs.
- **FIFO within a level.** Orders at one price sit in a `std::list<Order>`, giving time priority.
- **Iterator stability.** `std::list` keeps other iterators valid after a mid-list erase. The phonebook depends on this.
- **Sentinel prices.** Market orders use `INT64_MAX` (buy) or `0` (sell), so one matching routine handles both market and limit orders.
- **FOK dry run.** Available liquidity is computed before the book is touched, so a rejected FOK leaves no trace.
- **Pool allocation.** `Pool<T>` and `PoolAllocator<T>` serve the map and list nodes from a pre-allocated free list. The phonebook and trade log still use the default heap allocator.
- **Lock-free ingestion.** `RingQueue<T, N>` uses CAS to claim slots and per-slot ready flags to publish them. In `MatchingEngine` only the matcher thread touches the book. The `recursive_mutex` was removed in v7.0, so the book has a single writer and no locks. Because slots are published with per-slot ready flags, a producer that stalls between claiming and publishing a slot can delay the consumer at that slot.
- **Per-instrument routing.** ITCH messages are routed by `stock_locate` into separate `OrderBook` instances. ITCH Add messages use `OrderBook::restOrder()` to insert already-resting orders without triggering matching.

### Complexity

- Cancel: O(1) lookup + unlink; O(log n) to erase an emptied level.
- Match: O(k) in orders and levels consumed, plus O(log n) per level erased.
- Push (MPSC queue): O(1) amortized.

### Matching algorithm

```
addOrder(order):
    FOK:          if !can_fill(order): reject, else match
    Market / IOC: match; never rests
    Limit:        match; rest any leftover in the book
```

The match loop sweeps the opposite side one price level at a time and stops when the incoming order is fully filled, the opposite side is empty, or the next level no longer crosses the incoming price. A trade is recorded on every iteration.

## Project Structure

```
orderbook/
├── order.h              # Order, Side, Ordertype
├── pricelevel.h         # Orders resting at a single price
├── trader.h             # Trade record
├── pool.h               # Pool<T>: pre-allocated free list
├── poolallocator.h      # STL allocator interface over Pool<T>
├── orderbook.h          # Engine class declaration
├── orderbook.cpp        # Engine implementation
├── RingQueue.h          # Lock-free MPSC ring buffer
├── MatchingEngine.h     # Queue + matcher thread wrapper
├── itch_parser.cpp      # NASDAQ ITCH 5.0 binary feed parser
├── main.cpp             # Test suite (33 tests, 8 sections)
├── ring_test.cpp        # Ring queue FIFO test
├── mpsc_test.cpp        # Multi-producer correctness test
├── engine_test.cpp      # End-to-end pipeline test
├── bench_orderbook.cpp  # Google Benchmark harness
├── CMakeLists.txt       # Build system
├── LICENSE
└── README.md
```

## Build and Run

Requires CMake 3.14+, a C++17 compiler, and (optionally) Google Benchmark.

### Linux / macOS

```bash
# Dependencies
sudo pacman -S cmake gcc benchmark              # Arch
sudo apt install cmake g++ libbenchmark-dev     # Ubuntu / Debian

# Clone and build
git clone https://github.com/batmanSarv/Order_book.git
cd Order_book
mkdir build && cd build
cmake ..
make

# Run
./orderbook_tests         # 33 tests
./ring_test               # ring queue test
./mpsc_test               # multi-producer correctness
./engine_test             # end-to-end pipeline
./orderbook_bench         # benchmarks
./orderbook_itch <file>   # NASDAQ ITCH replay

# Tail latency benchmark, pinned to one core
taskset -c 1 ./orderbook_bench --benchmark_filter=BM_LatencyPercentiles
```

### Windows (MINGW64)

MINGW64 binaries link against `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll`, which are not on the default PATH. Either run from the MINGW64 terminal, or static-link with `-static -static-libgcc -static-libstdc++`.

```bash
pacman -S git cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-benchmark
git clone https://github.com/batmanSarv/Order_book.git
cd Order_book
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

./orderbook_tests.exe
./orderbook_bench.exe
./orderbook_itch.exe <file>
```

Manual build (no CMake):

```bash
g++ -std=c++17 orderbook.cpp main.cpp -o main && ./main
```

## Testing

33 hand-written scenarios across 8 sections, each followed by invariant checks.

| Section | Coverage |
| :--- | :--- |
| Basics | Add, cancel, best bid/ask, empty book |
| Matching | Full match, partial fill, no-cross, multi-level sweep |
| Market orders | Full fill, partial (discard), empty book |
| Trade log | Buy side, sell side, multi-trade IDs |
| Modify | Time-priority loss, price change, crossing modify, invalid |
| Depth snapshot | Top-N, truncation, over-request |
| IOC | Full fill, partial, price respect |
| FOK | Full fill, rejection, multi-level, price respect |

Invariants verified after every operation: the book is never crossed; each level's `total_quantity` equals the sum of its order quantities; no zero-quantity orders rest; every level in the map holds a non-empty order list.

## Performance

### Environment

| Item | Value |
| :--- | :--- |
| CPU | Intel Core i3-1115G4, 2 cores / 4 threads, base 3.00 GHz, max turbo 4.10 GHz |
| Compiler | g++ (GCC) 16.2.1, `-O3 -march=native -pthread` |
| Kernel | 7.2.7-arch1-1 |
| Turbo / SMT | Turbo ON, SMT ON |
| Governor | `performance` |
| Isolation | `isolcpus=1 nohz_full=1 rcu_nocbs=1` |
| Pinning | `taskset -c 1` |

All numbers come from one machine and synthetic workloads. Use them as relative comparisons only.

### Single-threaded (Google Benchmark, 50 repetitions, median)

| Benchmark | Median | CV | Throughput |
| :--- | :--- | :--- | :--- |
| Insert | 109 ns | 9.1% | 9.2M ops/s |
| Cancel | 96 ns | 9.9% | 10.5M ops/s |
| Match | 111 ns | 15.2% | 9.1M ops/s |
| Mixed add + cancel | 60.4 ns | 3.1% | 16.6M ops/s |

Insert = one non-crossing limit add into a pre-filled book. Cancel = one cancel of an existing order. Match = one market buy sweeping one ask level. Mixed = one add followed by one cancel of the same order. These are data-structure numbers only: no queue, no threads, no parser.

These figures have not been remeasured since the v7.0 mutex removal. The Mixed figure is lower than Insert alone because the add and cancel reuse the same pool node, which keeps it hot in cache; do not read it as a typical per-operation cost.

### Tail latency (100,000 limit orders)

Measured with `__rdtsc()` bracketed by `_mm_lfence()` to stop out-of-order execution from skewing the cycle count, instead of `std::chrono`, whose call overhead is a large fraction of a ~100 ns operation. Samples are recorded into a pre-allocated array and percentiles are computed after the loop. Pinned with `taskset -c 1`. Google Benchmark, 5 repetitions; median across repetitions reported.

| Metric | Median of 5 runs | Run-to-run CV | Notes |
| :--- | :--- | :--- | :--- |
| p50 | 119.5 ns | 0.73% | Stable |
| p90 | 141.6 ns | 1.6% | Stable |
| p99 | 199.0 ns | 4.1% | |
| p99.9 | 307.8 ns | 26.9% | Mean 362 ns; noisy. Likely cache misses, not confirmed with perf counters |
| Max | 32.0 µs | 92.6% | Mean 38.8 µs; dominated by OS interrupts / scheduling, not reproducible |

Timer overhead (`__rdtsc` + `_mm_lfence`, empty loop): ~9.3 ns, not subtracted from the figures above.

The p50 through p99 figures are reproducible. The p99.9 is indicative only, and the max is noise from the OS, not a property of the engine. The benchmark is closed-loop (one operation at a time), so coordinated omission does not apply, but it also means these are service times, not latencies under a fixed arrival rate.

### Multi-threaded

Wall time for 100,000 orders submitted across N producers: `OrderBook` (mutex-protected) vs `MatchingEngine` (lock-free MPSC queue + dedicated matcher thread).

| Producers | Mutex (v3) | MPSC queue (v5) | Speedup |
| :--- | :--- | :--- | :--- |
| 1 | 8.6 ms | 2.5 ms | 3.4× |
| 2 | 61.7 ms | 7.2 ms | 8.6× |
| 4 | 225.7 ms | 15.1 ms | 15.0× |

The mutex column was measured on v3; that code path no longer exists in the current build, so the baseline cannot be rebuilt from `main`.

### At-scale (single-threaded)

Book sizes from 1K to 5M orders, pre-filled then measured:

| Book size | Insert | Cancel | Match |
| :--- | :--- | :--- | :--- |
| 1K | 866 ns | 10 ns | 96 ns |
| 100K | 878 ns | 10 ns | 417 ns |
| 1M | 890 ns | 870 ns | 560 ns |
| 5M | 944 ns | 930 ns | 542 ns |

Cancel stays fast (~10 ns) while the working set fits in L3. Once the book exceeds cache, cancel jumps to ~900 ns: the O(1) algorithm is unchanged, the hardware is. Insert is dominated by cache misses on the red-black tree and stays roughly flat at ~900 ns across sizes.

## NASDAQ ITCH 5.0 Replay

Parses length-prefixed ITCH 5.0 files with per-instrument routing. Tested with a 50 MB slice of `07302019.NASDAQ_ITCH50`:

| Metric | Value |
| :--- | :--- |
| Messages parsed | 1,706,245 |
| Instruments | 1,727 |
| Add orders | 633,609 |
| Deletes | 621,525 |
| Cancels | 144,534 |
| Wall time | ~0.92 s |
| CPU time | ~0.40 s |

Verified with real tickers (ARGX, VOD, HSBC, MTL, PLD, HEI, SPY). All 1,727 books pass invariant checks after replay (no crossed books, no zero-quantity orders). That checks internal consistency, not equality with the real exchange book. The wall/CPU gap is file I/O: the parser reads the file into memory before processing.

## Optimization history

Single-threaded (mixed add + cancel):

| Step | Median | Kind |
| :--- | :--- | :--- |
| Baseline (`std::allocator`) | 93.4 ns | n/a |
| Pool allocator on map nodes | 85.9 ns | Code |
| Pool allocator on list nodes | 80.2 ns | Code |
| CPU governor set to `performance` | 60.4 ns | Environment |
| v7.0: `recursive_mutex` removed from `OrderBook` | not yet remeasured | Code |

Total: 93.4 ns → 60.4 ns (−35%). Most of the gain (80.2 → 60.4 ns) came from the governor, not code. The two allocator steps (−8%, −6%) are small next to the 9–15% run-to-run variation on single-operation benchmarks and per-step variation was not recorded, so treat them as indicative, not proven.

Adding `std::recursive_mutex` for thread safety (v3) raised the single-threaded mixed number to 94 ns, about 56% on the hot path. v7.0 removed it entirely: the matching engine is now a strict single-writer pipeline.

Milestones: v6.0 CMake build system; v6.1 NASDAQ ITCH 5.0 parser; v7.0 rdtsc tail-latency benchmark and `recursive_mutex` removal.

## Limitations & Future Work

1. **`std::list` pointer-chasing.** The pool allocator removes `malloc` for list nodes but FIFO traversal still misses cache. Future: intrusive list.
2. **`std::map` cache misses.** Red-black trees are not cache-friendly at scale. Future: B-tree or sorted flat array for price levels.
3. **Heap allocations remain** in the phonebook and trade log.
4. **No self-trade prevention.**
5. **No partial cancel** (modify is cancel + re-add).
6. **Single instrument per `OrderBook`**; multi-instrument routing happens in the parser.
7. **`RingQueue` is fixed-capacity (65536)**; `submit()` spins when full.
8. **ITCH coverage:** Add, Cancel, Delete only. Execute, Replace, and MPID Adds are not applied.
9. **No sanitizer runs** (ASan/UBSan/TSan) and **no differential test** against a reference engine.
10. **OS jitter:** max latency still reaches tens of µs and varies run to run, even with `isolcpus`. SMT is on, so the sibling hyperthread of the pinned core can add noise. Future: SMT off, `PREEMPT_RT`, or kernel bypass.
11. **No end-to-end pipeline latency** (parser → queue → book). The latency figures above cover the book only.

## License

MIT. See [`LICENSE`](LICENSE).
