cmake_minimum_required(VERSION 3.14)
project(orderbook CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O3 -march=native -pthread)

# ============================================================
# Test target — 33 hand-written scenarios
# ============================================================
add_executable(orderbook_tests
    orderbook.cpp
    main.cpp)

# ============================================================
# Standalone test programs
# ============================================================
add_executable(ring_test
    ring_test.cpp)

add_executable(mpsc_test
    mpsc_test.cpp)

add_executable(engine_test
    engine_test.cpp
    orderbook.cpp)

# ============================================================
# Benchmark target — requires Google Benchmark
# ============================================================
find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(orderbook_bench
        orderbook.cpp
        bench_orderbook.cpp)
    target_link_libraries(orderbook_bench PRIVATE benchmark::benchmark)
    message(STATUS "Google Benchmark found — bench target enabled")
else()
    message(STATUS "Google Benchmark not found — bench target skipped")
endif()
