#include <benchmark/benchmark.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kIncrementsPerThread = 1'000'000;

struct AdjacentCounters {
  std::atomic<std::uint64_t> a{0};
  std::atomic<std::uint64_t> b{0};
};

struct PaddedCounters {
  alignas(64) std::atomic<std::uint64_t> a{0};
  alignas(64) std::atomic<std::uint64_t> b{0};
};

void BM_FalseSharingAdjacent(benchmark::State& state) {
  static AdjacentCounters counters;

  for (auto _ : state) {
    std::atomic<std::uint64_t>& target =
        (state.thread_index() == 0) ? counters.a : counters.b;
    for (std::size_t i = 0; i < kIncrementsPerThread; ++i) {
      target.fetch_add(1, std::memory_order_relaxed);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kIncrementsPerThread * state.threads()));
  }
}

void BM_FalseSharingPadded(benchmark::State& state) {
  static PaddedCounters counters;

  for (auto _ : state) {
    std::atomic<std::uint64_t>& target =
        (state.thread_index() == 0) ? counters.a : counters.b;
    for (std::size_t i = 0; i < kIncrementsPerThread; ++i) {
      target.fetch_add(1, std::memory_order_relaxed);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kIncrementsPerThread * state.threads()));
  }
}

}  // namespace

BENCHMARK(BM_FalseSharingAdjacent)->Threads(2)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FalseSharingPadded)->Threads(2)->Unit(benchmark::kMicrosecond);
