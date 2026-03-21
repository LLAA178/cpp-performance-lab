#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr std::size_t kStepsPerIteration = 4 * 1024 * 1024;

std::vector<std::uint32_t> BuildRandomCycle(std::size_t n) {
  std::vector<std::uint32_t> order(n);
  std::iota(order.begin(), order.end(), 0U);
  std::mt19937 rng(12345);
  std::shuffle(order.begin(), order.end(), rng);

  std::vector<std::uint32_t> next(n);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    next[order[i]] = order[i + 1];
  }
  next[order.back()] = order.front();
  return next;
}

void BM_CacheLevels(benchmark::State& state) {
  const std::size_t bytes = static_cast<std::size_t>(state.range(0));
  const std::size_t entries = std::max<std::size_t>(bytes / sizeof(std::uint32_t), 1024);
  const std::vector<std::uint32_t> next = BuildRandomCycle(entries);
  std::uint32_t idx = 0;

  for (auto _ : state) {
    for (std::size_t step = 0; step < kStepsPerIteration; ++step) {
      idx = next[idx];
    }
    benchmark::DoNotOptimize(idx);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration * sizeof(std::uint32_t)));
  state.counters["working_set_kib"] = static_cast<double>(bytes / 1024);
}

}  // namespace

BENCHMARK(BM_CacheLevels)
    ->Arg(4 * 1024)
    ->Arg(8 * 1024)
    ->Arg(16 * 1024)
    ->Arg(24 * 1024)
    ->Arg(32 * 1024)
    ->Arg(48 * 1024)
    ->Arg(64 * 1024)
    ->Arg(96 * 1024)
    ->Arg(128 * 1024)
    ->Arg(192 * 1024)
    ->Arg(256 * 1024)
    ->Arg(384 * 1024)
    ->Arg(512 * 1024)
    ->Arg(768 * 1024)
    ->Arg(1024 * 1024)
    ->Arg(1536 * 1024)
    ->Arg(2048 * 1024)
    ->Arg(3072 * 1024)
    ->Arg(4 * 1024 * 1024)
    ->Arg(6 * 1024 * 1024)
    ->Arg(8 * 1024 * 1024)
    ->Arg(12 * 1024 * 1024)
    ->Arg(16 * 1024 * 1024)
    ->Arg(24 * 1024 * 1024)
    ->Arg(32 * 1024 * 1024)
    ->Arg(48 * 1024 * 1024)
    ->Arg(64 * 1024 * 1024)
    ->Unit(benchmark::kMicrosecond);
