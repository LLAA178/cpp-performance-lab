# cpp-performance-lab

A focused C++ microbenchmark repository for cache, memory, ILP, synchronization, queue, and allocator behavior.

## Purpose

- Build intuition for cache hierarchy and memory access patterns
- Compare synchronization and data-structure tradeoffs with reproducible microbenchmarks
- Produce evidence-based performance notes from stable runs

## 1) Prerequisites

- CMake >= 3.20
- A C++20 compiler (clang++ or g++)
- Git + internet access (for fetching `google/benchmark`)

## 2) Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 3) Run all benchmarks (recommended)

```bash
scripts/run_all.sh
```

## 4) Run a single benchmark

```bash
./build/benchmark/bm_stride_access --benchmark_min_time=0.3s
./build/benchmark/bm_pointer_chasing --benchmark_min_time=0.3s
./build/benchmark/bm_false_sharing --benchmark_min_time=0.3s
./build/benchmark/bm_aos_vs_soa --benchmark_min_time=0.3s
./build/benchmark/bm_mutex_vs_atomic --benchmark_min_time=0.3s
./build/benchmark/bm_cache_levels --benchmark_min_time=0.3s
./build/benchmark/bm_ilp --benchmark_min_time=0.3s
./build/benchmark/bm_cache_associativity --benchmark_min_time=0.3s
./build/benchmark/bm_queue \
  --benchmark_filter='BM_Queue(MutexTransfer/batch:64/backoff:0|SpscRingTransfer/batch:8/backoff:0)$' \
  --benchmark_min_time=1s \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
./build/benchmark/bm_memory_pool --benchmark_min_time=0.3s
```

Queue benchmark parameters:
- `batch`: producer-side batch enqueue size (`1/8/64`)
- `backoff`: `0=yield`, `1=spin`, `2=hybrid`

## 5) Repository layout

```text
cpp-performance-lab/
  CMakeLists.txt
  README.md
  results-summary.md
  benchmark/
    bm_stride_access.cpp
    bm_pointer_chasing.cpp
    bm_false_sharing.cpp
    bm_aos_vs_soa.cpp
    bm_mutex_vs_atomic.cpp
    bm_cache_levels.cpp
    bm_ilp.cpp
    bm_cache_associativity.cpp
    bm_queue.cpp
    bm_memory_pool.cpp
  scripts/
    run_all.sh
```

## 6) Experiments

- `bm_stride_access`: larger stride degrades locality and throughput
- `bm_pointer_chasing`: random pointer chasing becomes latency-bound
- `bm_false_sharing`: adjacent counters vs cache-line-padded counters
- `bm_aos_vs_soa`: data layout impact (AoS/SoA, narrow and wide-struct variants)
- `bm_mutex_vs_atomic`: contention behavior under 1/2/4 threads
- `bm_cache_levels`: working-set sweep to show cache/memory transition points
- `bm_ilp`: dependent vs independent instruction streams
- `bm_cache_associativity`: friendly stride vs conflict-prone stride
- `bm_queue`: mutex queue vs SPSC ring with tuned config
- `bm_memory_pool`: new/delete vs locked pool vs thread-local pool

## 7) Results

- `results-summary.md`: current run summary and conclusions
