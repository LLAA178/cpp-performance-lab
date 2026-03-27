#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr std::size_t kStepsPerIteration = 4 * 1024 * 1024;
constexpr std::size_t kPageBytes = 4096;
constexpr std::size_t kPages = 32768;

std::vector<std::uint32_t> BuildPageCycle(bool randomize) {
  std::vector<std::uint32_t> order(kPages);
  std::iota(order.begin(), order.end(), 0U);
  if (randomize) {
    std::mt19937 rng(12345);
    std::shuffle(order.begin(), order.end(), rng);
  }

  std::vector<std::uint32_t> next(kPages);
  for (std::size_t i = 0; i + 1 < order.size(); ++i) {
    next[order[i]] = order[i + 1];
  }
  next[order.back()] = order.front();
  return next;
}

const std::vector<std::uint32_t>& GetPageCycle(bool randomize) {
  static const std::vector<std::uint32_t> seq = BuildPageCycle(false);
  static const std::vector<std::uint32_t> rnd = BuildPageCycle(true);
  return randomize ? rnd : seq;
}

void BM_ContiguousPageWalk(benchmark::State& state) {
  const std::size_t ints_per_page = kPageBytes / sizeof(std::uint32_t);
  std::vector<std::uint32_t> data(kPages * ints_per_page);
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::size_t page = 0; page < kPages; ++page) {
      checksum += data[page * ints_per_page];
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kPages));
}

void BM_PageStrideWalk(benchmark::State& state) {
  const auto& next = GetPageCycle(false);
  const std::size_t ints_per_page = kPageBytes / sizeof(std::uint32_t);
  std::vector<std::uint32_t> data(kPages * ints_per_page);
  std::uint32_t idx = 0;
  for (auto _ : state) {
    for (std::size_t step = 0; step < kStepsPerIteration; ++step) {
      idx = next[idx];
      benchmark::DoNotOptimize(data[static_cast<std::size_t>(idx) * ints_per_page]);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration));
}

void BM_RandomPageWalk(benchmark::State& state) {
  const auto& next = GetPageCycle(true);
  const std::size_t ints_per_page = kPageBytes / sizeof(std::uint32_t);
  std::vector<std::uint32_t> data(kPages * ints_per_page);
  std::uint32_t idx = 0;
  for (auto _ : state) {
    for (std::size_t step = 0; step < kStepsPerIteration; ++step) {
      idx = next[idx];
      benchmark::DoNotOptimize(data[static_cast<std::size_t>(idx) * ints_per_page]);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration));
}

}  // namespace

BENCHMARK(BM_ContiguousPageWalk)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_PageStrideWalk)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_RandomPageWalk)->Unit(benchmark::kMillisecond);
