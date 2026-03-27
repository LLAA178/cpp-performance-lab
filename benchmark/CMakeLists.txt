add_executable(bm_stride_access cache/bm_stride_access.cpp)
target_link_libraries(bm_stride_access PRIVATE benchmark::benchmark_main)
target_compile_options(bm_stride_access PRIVATE -O3 -march=native)

add_executable(bm_pointer_chasing cache/bm_pointer_chasing.cpp)
target_link_libraries(bm_pointer_chasing PRIVATE benchmark::benchmark_main)
target_compile_options(bm_pointer_chasing PRIVATE -O3 -march=native)

add_executable(bm_false_sharing concurrency/bm_false_sharing.cpp)
target_link_libraries(bm_false_sharing PRIVATE benchmark::benchmark_main)
target_compile_options(bm_false_sharing PRIVATE -O3 -march=native)

add_executable(bm_aos_vs_soa layout/bm_aos_vs_soa.cpp)
target_link_libraries(bm_aos_vs_soa PRIVATE benchmark::benchmark_main)
target_compile_options(bm_aos_vs_soa PRIVATE -O3 -march=native)

add_executable(bm_mutex_vs_atomic concurrency/bm_mutex_vs_atomic.cpp)
target_link_libraries(bm_mutex_vs_atomic PRIVATE benchmark::benchmark_main)
target_compile_options(bm_mutex_vs_atomic PRIVATE -O3 -march=native)

add_executable(bm_cache_levels cache/bm_cache_levels.cpp)
target_link_libraries(bm_cache_levels PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cache_levels PRIVATE -O3 -march=native)

add_executable(bm_ilp cpu/bm_ilp.cpp)
target_link_libraries(bm_ilp PRIVATE benchmark::benchmark_main)
target_compile_options(bm_ilp PRIVATE -O3 -march=native)

add_executable(bm_branch_prediction cpu/bm_branch_prediction.cpp)
target_link_libraries(bm_branch_prediction PRIVATE benchmark::benchmark_main)
target_compile_options(bm_branch_prediction PRIVATE -O3 -march=native)

add_executable(bm_inlining_effects cpu/bm_inlining_effects.cpp)
target_link_libraries(bm_inlining_effects PRIVATE benchmark::benchmark_main)
target_compile_options(bm_inlining_effects PRIVATE -O3 -march=native)

add_executable(bm_cache_associativity cache/bm_cache_associativity.cpp)
target_link_libraries(bm_cache_associativity PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cache_associativity PRIVATE -O3 -march=native)

add_executable(bm_queue concurrency/bm_queue.cpp)
target_link_libraries(bm_queue PRIVATE benchmark::benchmark_main)
target_compile_options(bm_queue PRIVATE -O3 -march=native)

add_executable(bm_mpsc_mpmc_queue concurrency/bm_mpsc_mpmc_queue.cpp)
target_link_libraries(bm_mpsc_mpmc_queue PRIVATE benchmark::benchmark_main)
target_compile_options(bm_mpsc_mpmc_queue PRIVATE -O3 -march=native)

add_executable(bm_cv_vs_spin concurrency/bm_cv_vs_spin.cpp)
target_link_libraries(bm_cv_vs_spin PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cv_vs_spin PRIVATE -O3 -march=native)

add_executable(bm_lock_variants concurrency/bm_lock_variants.cpp)
target_link_libraries(bm_lock_variants PRIVATE benchmark::benchmark_main)
target_compile_options(bm_lock_variants PRIVATE -O3 -march=native)

add_executable(bm_queue_message_size concurrency/bm_queue_message_size.cpp)
target_link_libraries(bm_queue_message_size PRIVATE benchmark::benchmark_main)
target_compile_options(bm_queue_message_size PRIVATE -O3 -march=native)

add_executable(bm_memory_pool memory/bm_memory_pool.cpp)
target_link_libraries(bm_memory_pool PRIVATE benchmark::benchmark_main)
target_compile_options(bm_memory_pool PRIVATE -O3 -march=native)

add_executable(bm_tlb_pressure memory/bm_tlb_pressure.cpp)
target_link_libraries(bm_tlb_pressure PRIVATE benchmark::benchmark_main)
target_compile_options(bm_tlb_pressure PRIVATE -O3 -march=native)

add_executable(bm_cross_thread_free memory/bm_cross_thread_free.cpp)
target_link_libraries(bm_cross_thread_free PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cross_thread_free PRIVATE -O3 -march=native)

add_executable(bm_allocator_variants memory/bm_allocator_variants.cpp)
target_link_libraries(bm_allocator_variants PRIVATE benchmark::benchmark_main)
target_compile_options(bm_allocator_variants PRIVATE -O3 -march=native)

add_executable(bm_allocator_mixed_size memory/bm_allocator_mixed_size.cpp)
target_link_libraries(bm_allocator_mixed_size PRIVATE benchmark::benchmark_main)
target_compile_options(bm_allocator_mixed_size PRIVATE -O3 -march=native)

add_executable(bm_mmap_vs_read syscalls/bm_mmap_vs_read.cpp)
target_link_libraries(bm_mmap_vs_read PRIVATE benchmark::benchmark_main)
target_compile_options(bm_mmap_vs_read PRIVATE -O3 -march=native)

add_executable(bm_clock_overhead syscalls/bm_clock_overhead.cpp)
target_link_libraries(bm_clock_overhead PRIVATE benchmark::benchmark_main)
target_compile_options(bm_clock_overhead PRIVATE -O3 -march=native)

add_executable(bm_mmap_cow syscalls/bm_mmap_cow.cpp)
target_link_libraries(bm_mmap_cow PRIVATE benchmark::benchmark_main)
target_compile_options(bm_mmap_cow PRIVATE -O3 -march=native)

add_executable(bm_page_fault_mlock syscalls/bm_page_fault_mlock.cpp)
target_link_libraries(bm_page_fault_mlock PRIVATE benchmark::benchmark_main)
target_compile_options(bm_page_fault_mlock PRIVATE -O3 -march=native)

add_executable(bm_memory_order concurrency/bm_memory_order.cpp)
target_link_libraries(bm_memory_order PRIVATE benchmark::benchmark_main)
target_compile_options(bm_memory_order PRIVATE -O3 -march=native)

add_executable(bm_thread_affinity concurrency/bm_thread_affinity.cpp)
target_link_libraries(bm_thread_affinity PRIVATE benchmark::benchmark_main)
target_compile_options(bm_thread_affinity PRIVATE -O3 -march=native)

add_executable(bm_pipe_vs_shm ipc/bm_pipe_vs_shm.cpp)
target_link_libraries(bm_pipe_vs_shm PRIVATE benchmark::benchmark_main)
target_compile_options(bm_pipe_vs_shm PRIVATE -O3 -march=native)

add_executable(bm_socketpair_vs_pipe ipc/bm_socketpair_vs_pipe.cpp)
target_link_libraries(bm_socketpair_vs_pipe PRIVATE benchmark::benchmark_main)
target_compile_options(bm_socketpair_vs_pipe PRIVATE -O3 -march=native)

add_executable(bm_virtual_vs_template_dispatch language/bm_virtual_vs_template_dispatch.cpp)
target_link_libraries(bm_virtual_vs_template_dispatch PRIVATE benchmark::benchmark_main)
target_compile_options(bm_virtual_vs_template_dispatch PRIVATE -O3 -march=native)

add_executable(bm_std_function_vs_lambda language/bm_std_function_vs_lambda.cpp)
target_link_libraries(bm_std_function_vs_lambda PRIVATE benchmark::benchmark_main)
target_compile_options(bm_std_function_vs_lambda PRIVATE -O3 -march=native)

add_executable(bm_exception_vs_error_code language/bm_exception_vs_error_code.cpp)
target_link_libraries(bm_exception_vs_error_code PRIVATE benchmark::benchmark_main)
target_compile_options(bm_exception_vs_error_code PRIVATE -O3 -march=native)

add_executable(bm_variant_vs_virtual language/bm_variant_vs_virtual.cpp)
target_link_libraries(bm_variant_vs_virtual PRIVATE benchmark::benchmark_main)
target_compile_options(bm_variant_vs_virtual PRIVATE -O3 -march=native)

add_executable(bm_dynamic_cast_vs_tag language/bm_dynamic_cast_vs_tag.cpp)
target_link_libraries(bm_dynamic_cast_vs_tag PRIVATE benchmark::benchmark_main)
target_compile_options(bm_dynamic_cast_vs_tag PRIVATE -O3 -march=native)

add_executable(bm_aliasing_effects cpu/bm_aliasing_effects.cpp)
target_link_libraries(bm_aliasing_effects PRIVATE benchmark::benchmark_main)
target_compile_options(bm_aliasing_effects PRIVATE -O3 -march=native)

add_executable(bm_container_lookup containers/bm_container_lookup.cpp)
target_link_libraries(bm_container_lookup PRIVATE benchmark::benchmark_main)
target_compile_options(bm_container_lookup PRIVATE -O3 -march=native)

add_executable(bm_vector_deque_list containers/bm_vector_deque_list.cpp)
target_link_libraries(bm_vector_deque_list PRIVATE benchmark::benchmark_main)
target_compile_options(bm_vector_deque_list PRIVATE -O3 -march=native)

add_executable(bm_socket_loopback network/bm_socket_loopback.cpp)
target_link_libraries(bm_socket_loopback PRIVATE benchmark::benchmark_main)
target_compile_options(bm_socket_loopback PRIVATE -O3 -march=native)
