#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kOpsPerIteration = 128 * 1024 * 1024;

void BM_ILP_Dependent(benchmark::State& state) {
  static volatile std::uint64_t x0 = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerIteration; ++i) {
      x0 = x0 + 1;
      x0 = x0 + 1;
    }
  }

  benchmark::DoNotOptimize(x0);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kOpsPerIteration * 2));
}

void BM_ILP_Independent(benchmark::State& state) {
  static volatile std::uint64_t x0 = 0;
  static volatile std::uint64_t x1 = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerIteration; ++i) {
      x0 = x0 + 1;
      x1 = x1 + 1;
    }
  }

  benchmark::DoNotOptimize(x0);
  benchmark::DoNotOptimize(x1);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kOpsPerIteration * 2));
}

}  // namespace

BENCHMARK(BM_ILP_Dependent)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_ILP_Independent)->Unit(benchmark::kMicrosecond);
