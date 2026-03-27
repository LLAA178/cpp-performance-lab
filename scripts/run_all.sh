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
./build/benchmark/bm_branch_prediction --benchmark_min_time=0.2s
./build/benchmark/bm_inlining_effects --benchmark_min_time=0.2s
./build/benchmark/bm_cache_associativity --benchmark_min_time=0.3s
# Effective queue configs from local runs:
# - Mutex queue: batch=64, backoff=0
# - SPSC ring:  batch=8,  backoff=0
./build/benchmark/bm_queue \
  --benchmark_filter='BM_Queue(MutexTransfer/batch:64/backoff:0|SpscRingTransfer/batch:8/backoff:0)$' \
  --benchmark_min_time=1s \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
./build/benchmark/bm_mpsc_mpmc_queue --benchmark_min_time=0.2s
./build/benchmark/bm_cv_vs_spin --benchmark_min_time=0.2s
./build/benchmark/bm_lock_variants --benchmark_min_time=0.2s
./build/benchmark/bm_queue_message_size --benchmark_min_time=0.2s
./build/benchmark/bm_memory_pool --benchmark_min_time=0.3s
./build/benchmark/bm_tlb_pressure --benchmark_min_time=0.2s
./build/benchmark/bm_cross_thread_free --benchmark_min_time=0.2s
./build/benchmark/bm_allocator_variants --benchmark_min_time=0.2s
./build/benchmark/bm_allocator_mixed_size --benchmark_min_time=0.2s
./build/benchmark/bm_vector_deque_list --benchmark_min_time=0.2s
./build/benchmark/bm_mmap_vs_read --benchmark_min_time=0.2s
./build/benchmark/bm_clock_overhead --benchmark_min_time=0.2s
./build/benchmark/bm_mmap_cow --benchmark_min_time=0.2s
./build/benchmark/bm_page_fault_mlock --benchmark_min_time=0.2s
./build/benchmark/bm_memory_order --benchmark_min_time=0.2s
./build/benchmark/bm_thread_affinity --benchmark_min_time=0.2s
./build/benchmark/bm_pipe_vs_shm --benchmark_min_time=0.2s
./build/benchmark/bm_socketpair_vs_pipe --benchmark_min_time=0.2s
./build/benchmark/bm_virtual_vs_template_dispatch --benchmark_min_time=0.2s
./build/benchmark/bm_std_function_vs_lambda --benchmark_min_time=0.2s
./build/benchmark/bm_exception_vs_error_code --benchmark_min_time=0.2s
./build/benchmark/bm_variant_vs_virtual --benchmark_min_time=0.2s
./build/benchmark/bm_dynamic_cast_vs_tag --benchmark_min_time=0.2s
./build/benchmark/bm_aliasing_effects --benchmark_min_time=0.2s
./build/benchmark/bm_container_lookup --benchmark_min_time=0.2s
./build/benchmark/bm_socket_loopback --benchmark_min_time=0.2s
