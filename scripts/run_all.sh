#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/benchmark/bm_stride_access --benchmark_min_time=0.3s
./build/benchmark/bm_pointer_chasing --benchmark_min_time=0.3s
./build/benchmark/bm_false_sharing --benchmark_min_time=0.3s
./build/benchmark/bm_aos_vs_soa --benchmark_min_time=0.3s
./build/benchmark/bm_mutex_vs_atomic --benchmark_min_time=0.3s
./build/benchmark/bm_cache_levels --benchmark_min_time=0.3s
./build/benchmark/bm_ilp --benchmark_min_time=0.3s
./build/benchmark/bm_cache_associativity --benchmark_min_time=0.3s
# Effective queue configs from local runs:
# - Mutex queue: batch=64, backoff=0
# - SPSC ring:  batch=8,  backoff=0
./build/benchmark/bm_queue \
  --benchmark_filter='BM_Queue(MutexTransfer/batch:64/backoff:0|SpscRingTransfer/batch:8/backoff:0)$' \
  --benchmark_min_time=1s \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
./build/benchmark/bm_memory_pool --benchmark_min_time=0.3s
