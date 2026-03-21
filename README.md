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

## 4) Run a specific benchmark

Standard pattern:

```bash
./build/benchmark/<binary_name> --benchmark_min_time=0.3s
```

Examples:

```bash
./build/benchmark/bm_stride_access --benchmark_min_time=0.3s
./build/benchmark/bm_cache_levels --benchmark_min_time=0.3s
```

Queue tuned run:

```bash
./build/benchmark/bm_queue \
  --benchmark_filter='BM_Queue(MutexTransfer/batch:64/backoff:0|SpscRingTransfer/batch:8/backoff:0)$' \
  --benchmark_min_time=1s \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
```

## 5) Benchmarks

- `bm_stride_access`: locality loss from larger access stride
- `bm_pointer_chasing`: sequential access vs irregular pointer traversal
- `bm_false_sharing`: adjacent counters vs cache-line-padded counters
- `bm_aos_vs_soa`: layout sensitivity for dense vs sparse field usage
- `bm_mutex_vs_atomic`: contention scaling for shared counter updates
- `bm_cache_levels`: throughput drop as working set crosses cache levels
- `bm_ilp`: dependent vs independent instruction streams
- `bm_cache_associativity`: friendly stride vs conflict-prone stride
- `bm_queue`: mutex queue vs tuned SPSC ring transfer
- `bm_memory_pool`: `new/delete` vs locked pool vs thread-local pool

## 6) Outputs

- `results-summary.md`: current run summary and conclusions

Generate figures from benchmark runs:

```bash
python3 scripts/generate_plots.py
```

## 7) Figures

### Cache Levels

![Cache levels](assets/plots/cache_levels_curve.png)

### Cache Associativity

![Associativity](assets/plots/associativity_conflict.png)

### Queue and Memory Pool

![Queue and memory pool](assets/plots/queue_memorypool.png)
