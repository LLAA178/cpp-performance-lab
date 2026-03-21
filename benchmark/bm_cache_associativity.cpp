#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t kStepsPerIteration = 8 * 1024 * 1024;
constexpr std::size_t kLineBytes = 64;
// Typical alias distance for many L1D caches (cache_size / associativity).
constexpr std::size_t kConflictStrideBytes = 8 * 1024;

void TouchPattern(benchmark::State& state, std::size_t stride_bytes) {
  const std::size_t lines = static_cast<std::size_t>(state.range(0));
  const std::size_t bytes = lines * stride_bytes;
  std::vector<std::uint8_t> data(bytes, 0);
  std::size_t idx = 0;

  for (auto _ : state) {
    for (std::size_t step = 0; step < kStepsPerIteration; ++step) {
      data[idx]++;
      idx += stride_bytes;
      if (idx >= data.size()) {
        idx -= data.size();
      }
    }
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kStepsPerIteration));
  state.counters["lines"] = static_cast<double>(lines);
  state.counters["stride_bytes"] = static_cast<double>(stride_bytes);
}

void BM_AssocFriendly(benchmark::State& state) {
  TouchPattern(state, kLineBytes);
}

void BM_AssocConflict(benchmark::State& state) {
  TouchPattern(state, kConflictStrideBytes);
}

}  // namespace

BENCHMARK(BM_AssocFriendly)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_AssocConflict)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)->Unit(benchmark::kMicrosecond);

