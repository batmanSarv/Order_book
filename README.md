# Order Book Matching Engine

A C++17 limit order book matching engine with a lock-free MPSC queue and
a NASDAQ ITCH 5.0 binary feed parser.

Supports limit, market, IOC, and FOK orders; multi-level price sweeps; a
trade log; and a custom pool allocator for the book's map and list nodes.

**C++17** · **MIT License**

---

## Overview

A ground-up matching engine built to work through low-level systems
concerns: memory layout, iterator stability, allocator behavior, lock-free
queue design, and benchmarking methodology.

**Three operating modes:**

- **Single-threaded (`OrderBook`)** — direct method calls.
- **Multi-threaded (`MatchingEngine`)** — producers push into a lock-free
  MPSC ring queue; a dedicated matcher thread consumes and processes.
- **ITCH replay (`itch_parser`)** — reads NASDAQ ITCH 5.0 binary feeds and
  routes Add / Cancel / Delete messages into per-instrument books.

---

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
| Lock-free MPSC ring queue | ✅ |
| Dedicated matcher thread | ✅ |
| NASDAQ ITCH 5.0 parser | ✅ |

---

## Design

### Data structures

| Member | Type | Purpose |
|--------|------|---------|
| `bids` | `std::map<int64_t, PriceLevel, std::greater<>, PoolAllocator<...>>` | Buy side, best (highest) price first |
| `asks` | `std::map<int64_t, PriceLevel, std::less<>, PoolAllocator<...>>` | Sell side, best (lowest) price first |
| `phonebook` | `std::unordered_map<uint64_t, std::list<Order, PoolAllocator<Order>>::iterator>` | Order ID → list iterator for cancel/modify |
| `trade` | `std::vector<Trade>` | Append-only execution log |
| `queue_` | `RingQueue<Order, 65536>` | Lock-free MPSC queue between producers and matcher |

### Key decisions

- **Integer prices.** `int64_t` ticks, avoiding floating-point rounding bugs.
- **FIFO within a level.** Orders at one price sit in a `std::list<Order>`,
  giving time priority.
- **Iterator stability.** `std::list` keeps other iterators valid after a
  mid-list erase. The phonebook depends on this.
- **Sentinel prices.** Market orders use `INT64_MAX` (buy) or `0` (sell),
  so one matching routine handles both market and limit orders.
- **FOK dry run.** Available liquidity is computed before the book is
  touched, so a rejected FOK leaves no trace.
- **Pool allocation.** `Pool<T>` and `PoolAllocator<T>` serve the map and
  list nodes from a pre-allocated free list.
- **Lock-free queue.** `RingQueue<T, N>` uses CAS to claim slots and
  per-slot ready flags to publish them.
- **Per-instrument routing.** ITCH messages are routed by `stock_locate`
  into separate `OrderBook` instances.

### Complexity

- **Cancel:** O(1) lookup + unlink. O(log n) to erase an emptied level.
- **Match:** O(k) in orders and levels consumed, plus O(log n) per level erased.
- **Push (MPSC queue):** O(1) amortized.

### Matching algorithm

```
addOrder(order):
    FOK:          if !can_fill(order): reject, else match
    Market / IOC: match; never rests
    Limit:        match; rest any leftover in the book
```

The match loop sweeps the opposite side one price level at a time and
stops when:

1. The incoming order is fully filled, or
2. The opposite side is empty, or
3. The next level no longer crosses the incoming price.

A trade is recorded on every iteration.

---

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

---

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
```

### Windows (MINGW64)

> **⚠️ Windows requires more runtime libraries than Linux.**
>
> The MINGW64 toolchain does not ship a shared C++ runtime. Binaries built
> with `g++` link against `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and
> `libwinpthread-1.dll`, which are **not** in the system PATH by default.
> Running a compiled `.exe` from Explorer without these DLLs fails silently.
>
> **Two ways to handle this:**
>
> 1. **Always run from the MINGW64 terminal.** The terminal's PATH includes
>    the MINGW64 `bin/` directory, so the DLLs resolve automatically.
> 2. **Static-link** by adding `-static -static-libgcc -static-libstdc++`
>    to the g++ command. The resulting `.exe` is self-contained.

```bash
# From MSYS2 MINGW64 shell — install everything required
pacman -S git cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-benchmark

# Clone and build
git clone https://github.com/batmanSarv/Order_book.git
cd Order_book
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

# Run from MINGW64 terminal
./orderbook_tests.exe
./orderbook_bench.exe
./orderbook_itch.exe <file>
```

**If you must run from Explorer**, rebuild with static linking:

```bash
g++ -std=c++17 -static -static-libgcc -static-libstdc++ \
    orderbook.cpp main.cpp -o orderbook_tests.exe
```

### Manual build (no CMake)

```bash
g++ -std=c++17 orderbook.cpp main.cpp -o main
./main
```

---

## Testing

33 hand-written scenarios across 8 sections, each followed by invariant
checks.

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

**Invariants verified after every operation:**

1. The book is never crossed (`best_bid < best_ask` when both sides non-empty).
2. Each level's `total_quantity` equals the sum of its order quantities.
3. No zero-quantity orders rest in the book.
4. Every level in the map holds a non-empty order list.

---

## Performance

### Environment

| Item | Value |
|------|-------|
| CPU | Intel Core i3-1115G4 — 2 physical cores / 4 threads, base 3.00 GHz, max turbo 4.10 GHz |
| Compiler | g++ (GCC) 16.2.1 |
| Kernel | 7.2.7-arch1-1 |
| Turbo / SMT | Turbo ON, SMT ON (2 threads per core) |
| Governor | `performance` |
| Pinning | `taskset -c 1` |
| Build | `-O3 -march=native -pthread` |

Google Benchmark, 50 repetitions, median reported.

### Single-threaded

| Benchmark | Median | CV | Throughput |
|-----------|--------|-----|------------|
| Insert | 109 ns | 9.1% | 9.2M ops/s |
| Cancel | 96 ns | 9.9% | 10.5M ops/s |
| Match | 111 ns | 15.2% | 9.1M ops/s |
| Mixed add + cancel | 60.4 ns | 3.1% | 16.6M ops/s |

**One iteration of each benchmark is:**

- **Insert** — one add of a non-crossing limit order into a pre-filled book.
- **Cancel** — one cancel of an existing order.
- **Match** — one market buy sweeping one ask level.
- **Mixed** — one add followed by one cancel of the same order.

The reported time is per iteration (one operation, or one add + one cancel
for Mixed).

### Multi-threaded

Wall time for **100,000 orders submitted across N producers**, measured
with `MatchingEngine` (lock-free MPSC queue + dedicated matcher thread)
vs `OrderBook` (mutex-protected).

| Producers | Mutex (v3) | MPSC queue (v5) | Speedup |
|-----------|------------|-----------------|---------|
| 1 | 8.6 ms | 2.5 ms | 3.4× |
| 2 | 61.7 ms | 7.2 ms | 8.6× |
| 4 | 225.7 ms | 15.1 ms | 15.0× |

The mutex version collapses under contention — throughput drops sharply as
producers increase. The MPSC queue holds up much better across producer
counts.

### At-scale (single-threaded)

Book sizes from 1K to 5M orders, pre-filled then measured:

| Book size | Insert | Cancel | Match |
|-----------|--------|--------|-------|
| 1K | 866 ns | 10 ns | 96 ns |
| 100K | 878 ns | 10 ns | 417 ns |
| 1M | 890 ns | 870 ns | 560 ns |
| 5M | 944 ns | 930 ns | 542 ns |

Cancel stays fast (~10 ns) while the working set fits in L3. Once the book
exceeds cache, cancel jumps to ~900 ns — the O(1) algorithm is unchanged,
the hardware is. Insert is dominated by cache misses on the red-black tree
and stays roughly flat at ~900 ns across sizes. Match grows sub-linearly
with the number of distinct price levels swept.

### NASDAQ ITCH 5.0 Replay

Parses length-prefixed NASDAQ ITCH 5.0 files with per-instrument routing.
Handles Add, Cancel, and Delete messages.

Tested with a 50 MB slice of `07302019.NASDAQ_ITCH50`:

| Metric | Value |
|--------|-------|
| Messages parsed | 1,706,245 |
| Instruments | 1,727 |
| Add orders | 633,609 |
| Deletes | 621,525 |
| Cancels | 144,534 |
| Wall time | ~0.92 s |
| CPU time | ~0.40 s |

Verified with real NASDAQ tickers (ARGX, VOD, HSBC, MTL, PLD, HEI, SPY).
All 1,727 books pass invariant checks after replay. The gap between wall
time and CPU time is file I/O — the parser reads the file into memory
before processing.

### Optimization history

Two separate tracks. Single-threaded improvements and multi-threaded
scaling. Both measured on the same i3-1115G4.

**Single-threaded (mixed add + cancel):**

| Step | Median | Kind |
|------|--------|------|
| Baseline (`std::allocator`) | 93.4 ns | n/a |
| Pool allocator on map nodes | 85.9 ns | Code |
| Pool allocator on list nodes | 80.2 ns | Code |
| CPU governor set to `performance` | 60.4 ns | Environment |

Total: 93.4 ns → 60.4 ns (−35%). Most of that gain (80.2 → 60.4 ns) came
from the governor setting, not from code. The two allocator steps (−8%, −6%)
are small next to the 9–15% run-to-run variation seen on the single-operation
benchmarks, and per-step variation was not recorded, so treat them as
indicative rather than proven.

**Single-threaded with mutex (v3+):**

After the `std::recursive_mutex` was added to `OrderBook` for thread safety,
the single-threaded mixed number rose to 94 ns. The mutex costs ~56% on the
single-thread hot path. It is included in v3–v6.

**Multi-threaded scaling (mutex vs MPSC queue):**

| Producers | Mutex (v3) | MPSC queue (v5) | Speedup |
|-----------|------------|-----------------|---------|
| 1 | 8.6 ms | 2.5 ms | 3.4× |
| 2 | 61.7 ms | 7.2 ms | 8.6× |
| 4 | 225.7 ms | 15.1 ms | 15.0× |

Wall time for 100,000 orders submitted across N producers. The mutex
version collapses under contention — throughput drops sharply as producers
increase. The MPSC queue holds up much better across producer counts.

**Feature milestones (no perf change):**

- **v6.0** — CMake build system
- **v6.1** — NASDAQ ITCH 5.0 parser (1.7M messages replayed in ~0.92 s)

Numbers come from one machine and synthetic workloads. Use them as
relative comparisons only. Tail latency (p99, p99.9) has not been measured.

---

## Limitations

- Single-instrument per `OrderBook`. Multi-instrument routing at the parser.
- No self-trade prevention.
- No partial cancel (modify is cancel + re-add).
- Phonebook and trade log use the default heap allocator.
- `RingQueue` is fixed-capacity (65536); `submit()` spins if full.
- No sanitizer runs (ASan/UBSan/TSan) yet.
- No differential test against a reference engine.
- Latency reported as medians only; no tail percentiles.
- ITCH parser handles 3 message types; Order Replace, MPID Adds, and
  Execute are not applied.

---

## License

MIT. See `LICENSE`.
