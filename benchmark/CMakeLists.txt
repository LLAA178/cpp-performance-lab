add_executable(bm_stride_access bm_stride_access.cpp)
target_link_libraries(bm_stride_access PRIVATE benchmark::benchmark_main)
target_compile_options(bm_stride_access PRIVATE -O3 -march=native)

add_executable(bm_pointer_chasing bm_pointer_chasing.cpp)
target_link_libraries(bm_pointer_chasing PRIVATE benchmark::benchmark_main)
target_compile_options(bm_pointer_chasing PRIVATE -O3 -march=native)

add_executable(bm_false_sharing bm_false_sharing.cpp)
target_link_libraries(bm_false_sharing PRIVATE benchmark::benchmark_main)
target_compile_options(bm_false_sharing PRIVATE -O3 -march=native)

add_executable(bm_aos_vs_soa bm_aos_vs_soa.cpp)
target_link_libraries(bm_aos_vs_soa PRIVATE benchmark::benchmark_main)
target_compile_options(bm_aos_vs_soa PRIVATE -O3 -march=native)

add_executable(bm_mutex_vs_atomic bm_mutex_vs_atomic.cpp)
target_link_libraries(bm_mutex_vs_atomic PRIVATE benchmark::benchmark_main)
target_compile_options(bm_mutex_vs_atomic PRIVATE -O3 -march=native)

add_executable(bm_cache_levels bm_cache_levels.cpp)
target_link_libraries(bm_cache_levels PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cache_levels PRIVATE -O3 -march=native)

add_executable(bm_ilp bm_ilp.cpp)
target_link_libraries(bm_ilp PRIVATE benchmark::benchmark_main)
target_compile_options(bm_ilp PRIVATE -O3 -march=native)

add_executable(bm_cache_associativity bm_cache_associativity.cpp)
target_link_libraries(bm_cache_associativity PRIVATE benchmark::benchmark_main)
target_compile_options(bm_cache_associativity PRIVATE -O3 -march=native)

add_executable(bm_queue bm_queue.cpp)
target_link_libraries(bm_queue PRIVATE benchmark::benchmark_main)
target_compile_options(bm_queue PRIVATE -O3 -march=native)

add_executable(bm_memory_pool bm_memory_pool.cpp)
target_link_libraries(bm_memory_pool PRIVATE benchmark::benchmark_main)
target_compile_options(bm_memory_pool PRIVATE -O3 -march=native)
