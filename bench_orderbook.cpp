#include <atomic>
#include <benchmark/benchmark.h>
#include "orderbook.h"
#include <random>
#include <vector>
#include <thread>
#include "MatchingEngine.h"

// ============================================================
// BM_AddAndCancel — the mixed workload you already have
// ============================================================
static void BM_AddAndCancel(benchmark::State& state) {
    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;

    for (auto _ : state) {
        int64_t price = mid + (rng() % 11) - 5;
        int64_t quantity = (rng() % 1000) + 1;
        Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
        Order o{ id_counter++, 1, price, quantity, 0, side, Ordertype::Limit };
        book.addOrder(o);
        book.cancelOrder(o.order_id);
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// BM_PureInsert — wide spread, no matching, growing book
// Resets the book every 50k inserts to stay under pool capacity
// ============================================================
static void BM_PureInsert(benchmark::State& state) {
    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;
    size_t inserted = 0;
    const size_t reset_every = 50000;

    for (auto _ : state) {
        if (inserted >= reset_every) {
            state.PauseTiming();
            book.clear();
            inserted = 0;
            state.ResumeTiming();
        }

        int64_t price = mid + (rng() % 2000) - 1000;   // wide spread
        Order o{ id_counter++, 1, price, 1, 0, Side::Buy, Ordertype::Limit };
        book.addOrder(o);
        ++inserted;
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// BM_PureCancel — pre-fill book, then cancel one per iteration
// Refills in batches of 50k when exhausted
// ============================================================
// ============================================================
// BM_PureCancel — pre-fill on first iteration (paused), then
// cancel one per iteration. Refills when batch is exhausted.
// ============================================================
static void BM_PureCancel(benchmark::State& state) {
    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;
    const size_t batch = 50000;

    std::vector<uint64_t> pending;

    for (auto _ : state) {
        if (pending.empty()) {
            state.PauseTiming();
            for (size_t i = 0; i < batch; ++i) {
                int64_t price = mid + (rng() % 2000) - 1000;
                Order o{ id_counter++, 1, price, 1, 0, Side::Buy, Ordertype::Limit };
                book.addOrder(o);
                pending.push_back(o.order_id);
            }
            state.ResumeTiming();
        }

        book.cancelOrder(pending.back());
        pending.pop_back();
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// BM_PureMatch — pre-fill asks on first iteration (paused),
// then send one crossing market buy per iteration.
// ============================================================
static void BM_PureMatch(benchmark::State& state) {
    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;
    const size_t batch = 50000;

    size_t asks_remaining = 0;

    for (auto _ : state) {
        if (asks_remaining == 0) {
            state.PauseTiming();
            for (size_t i = 0; i < batch; ++i) {
                int64_t price = mid + (rng() % 100);
                Order o{ id_counter++, 1, price, 10, 0, Side::Sell, Ordertype::Limit };
                book.addOrder(o);
            }
            asks_remaining = batch;
            state.ResumeTiming();
        }

        Order o{ id_counter++, 1, 0, 10, 0, Side::Buy, Ordertype::Market };
        book.addOrder(o);
        --asks_remaining;
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}
static void BM_MultiThreadedAddCancel(benchmark::State& state) {
    const int num_threads = static_cast<int>(state.range(0));

    OrderBook book;
    std::mt19937_64 rng(42);
    const int64_t mid = 100000;
    const int64_t ops_per_thread = 100000;

    for (auto _ : state) {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        std::atomic<uint64_t> id_counter{1};

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&book, &id_counter, mid, ops_per_thread, t]() {
                std::mt19937_64 local_rng(42 + t);   // per-thread seed
                for (int64_t i = 0; i < ops_per_thread; ++i) {
                    int64_t price = mid + (local_rng() % 11) - 5;
                    int64_t qty = (local_rng() % 1000) + 1;
                    Side side = (local_rng() % 2 == 0) ? Side::Buy : Side::Sell;
                    uint64_t id = id_counter.fetch_add(1);

                    Order o{id, 1, price, qty, 0, side, Ordertype::Limit};
                    book.addOrder(o);
                    book.cancelOrder(o.order_id);
                }
            });
        }

        for (auto& th : threads) th.join();
    }

    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations() * num_threads * ops_per_thread);
}

// ============================================================
// BM_QueueProducer — submit N orders through the lock-free queue,
// the matcher processes them in a background thread
// ============================================================
static void BM_QueueProducer(benchmark::State& state) {
    const int64_t N = 100000;
    const int64_t mid = 100000;

    for (auto _ : state) {
        MatchingEngine engine;
        std::mt19937_64 rng(42);

        for (int64_t i = 0; i < N; ++i) {
            int64_t price = mid + (rng() % 11) - 5;
            int64_t qty = (rng() % 1000) + 1;
            Side side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
            Order o{static_cast<uint64_t>(i + 1), 1, price, qty, 0, side, Ordertype::Limit};
            engine.submit(o);
        }
        // engine destructor joins the matcher — this includes drain time
    }
    state.SetItemsProcessed(state.iterations() * N);
}

// ============================================================
// BM_MultiProducerQueue — N producers push through the MPSC queue,
// one matcher thread consumes
// ============================================================
static void BM_MultiProducerQueue(benchmark::State& state) {
    const int num_producers = static_cast<int>(state.range(0));
    const int64_t ops_per_producer = 25000;

    for (auto _ : state) {
        MatchingEngine engine;
        std::atomic<uint64_t> id_counter{1};

        std::vector<std::thread> producers;
        producers.reserve(num_producers);

        for (int t = 0; t < num_producers; ++t) {
            producers.emplace_back([&, t]() {
                std::mt19937_64 local_rng(42 + t);
                for (int64_t i = 0; i < ops_per_producer; ++i) {
                    int64_t price = 100000 + (local_rng() % 11) - 5;
                    int64_t qty = (local_rng() % 1000) + 1;
                    Side side = (local_rng() % 2 == 0) ? Side::Buy : Side::Sell;
                    uint64_t id = id_counter.fetch_add(1);

                    Order o{id, 1, price, qty, 0, side, Ordertype::Limit};
                    engine.submit(o);
                }
            });
        }

        for (auto& p : producers) p.join();
        // engine destructor joins matcher
    }
    state.SetItemsProcessed(state.iterations() * num_producers * ops_per_producer);
}
// ============================================================
// BM_LargeBookInsert — pre-fill to N, then measure insert
// ============================================================
static void BM_LargeBookInsert(benchmark::State& state) {
    const size_t prefill = static_cast<size_t>(state.range(0));

    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;

    // Pre-fill with wide spread — no matching
    for (size_t i = 0; i < prefill; ++i) {
        int64_t price = mid + static_cast<int64_t>(rng() % 200000) - 100000;
        Order o{id_counter++, 1, price, 1, 0, Side::Buy, Ordertype::Limit};
        book.addOrder(o);
    }

    for (auto _ : state) {
        int64_t price = mid + static_cast<int64_t>(rng() % 200000) - 100000;
        Order o{id_counter++, 1, price, 1, 0, Side::Buy, Ordertype::Limit};
        book.addOrder(o);
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// BM_LargeBookCancel — pre-fill to N, then measure cancel
// ============================================================
static void BM_LargeBookCancel(benchmark::State& state) {
    const size_t prefill = static_cast<size_t>(state.range(0));

    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;

    std::vector<uint64_t> ids;
    ids.reserve(prefill);

    for (size_t i = 0; i < prefill; ++i) {
        int64_t price = mid + static_cast<int64_t>(rng() % 200000) - 100000;
        Order o{id_counter++, 1, price, 1, 0, Side::Buy, Ordertype::Limit};
        book.addOrder(o);
        ids.push_back(o.order_id);
    }

    size_t idx = 0;
    for (auto _ : state) {
        if (idx >= ids.size()) {
            state.PauseTiming();
            idx = 0;
            state.ResumeTiming();
        }
        book.cancelOrder(ids[idx++]);
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// BM_LargeBookMatch — pre-fill asks, then send matching buys
// ============================================================
static void BM_LargeBookMatch(benchmark::State& state) {
    const size_t prefill = static_cast<size_t>(state.range(0));

    OrderBook book;
    std::mt19937_64 rng(42);
    uint64_t id_counter = 1;
    const int64_t mid = 100000;

    // Pre-fill asks at various prices
    for (size_t i = 0; i < prefill; ++i) {
        int64_t price = mid + static_cast<int64_t>(rng() % 200000);
        Order o{id_counter++, 1, price, 10, 0, Side::Sell, Ordertype::Limit};
        book.addOrder(o);
    }

    size_t matched = 0;
    for (auto _ : state) {
        if (matched >= prefill) {
            state.PauseTiming();
            // refill
            for (size_t i = 0; i < prefill; ++i) {
                int64_t price = mid + static_cast<int64_t>(rng() % 200000);
                Order o{id_counter++, 1, price, 10, 0, Side::Sell, Ordertype::Limit};
                book.addOrder(o);
            }
            matched = 0;
            state.ResumeTiming();
        }
        // Market buy — always matches best ask
        Order o{id_counter++, 1, 0, 10, 0, Side::Buy, Ordertype::Market};
        book.addOrder(o);
        ++matched;
    }
    benchmark::DoNotOptimize(book);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================
// ============================================================
BENCHMARK(BM_AddAndCancel);
BENCHMARK(BM_PureInsert);
BENCHMARK(BM_PureCancel);
BENCHMARK(BM_PureMatch);
BENCHMARK(BM_MultiThreadedAddCancel)->Arg(1)->Arg(2)->Arg(4);
BENCHMARK(BM_QueueProducer);
BENCHMARK(BM_MultiProducerQueue)->Arg(1)->Arg(2)->Arg(4);
BENCHMARK(BM_LargeBookInsert)->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(5000000);
BENCHMARK(BM_LargeBookCancel)->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(5000000);
BENCHMARK(BM_LargeBookMatch)->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(5000000);
BENCHMARK_MAIN();
