#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr std::size_t kWorkingSet = 16 * 1024 * 1024;  // 16M entries

std::vector<std::uint32_t> BuildRandomCycle(std::size_t n) {
  std::vector<std::uint32_t> order(n);
  std::iota(order.begin(), order.end(), 0U);
  std::mt19937 rng(42);
  std::shuffle(order.begin(), order.end(), rng);

  std::vector<std::uint32_t> next(n);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    next[order[i]] = order[i + 1];
  }
  next[order.back()] = order.front();
  return next;
}

void BM_ArraySequential(benchmark::State& state) {
  std::vector<std::uint32_t> data(kWorkingSet, 1);
  std::uint64_t sum = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < data.size(); ++i) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(data.size()));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(data.size() * sizeof(std::uint32_t)));
}

void BM_PointerChasing(benchmark::State& state) {
  const std::vector<std::uint32_t> next = BuildRandomCycle(kWorkingSet);
  std::uint32_t idx = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < next.size(); ++i) {
      idx = next[idx];
    }
    benchmark::DoNotOptimize(idx);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(next.size()));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(next.size() * sizeof(std::uint32_t)));
}

}  // namespace

BENCHMARK(BM_ArraySequential)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_PointerChasing)->Unit(benchmark::kMicrosecond);
