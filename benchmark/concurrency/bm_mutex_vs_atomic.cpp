#include <benchmark/benchmark.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {

constexpr std::size_t kOpsPerThread = 500'000;

void BM_AtomicCounter(benchmark::State& state) {
  static std::atomic<std::uint64_t> counter{0};

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
  }
}

void BM_MutexCounter(benchmark::State& state) {
  static std::mutex m;
  static std::uint64_t counter = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      std::lock_guard<std::mutex> lock(m);
      ++counter;
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
  }
}

}  // namespace

BENCHMARK(BM_AtomicCounter)->Threads(1)->Threads(2)->Threads(4)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_MutexCounter)->Threads(1)->Threads(2)->Threads(4)->Unit(benchmark::kMicrosecond);

