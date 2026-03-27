#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

void BM_StrideAccess(benchmark::State& state) {
  constexpr std::size_t kArraySize = 64 * 1024 * 1024;  // 64M ints ~= 256MB
  const std::size_t stride = static_cast<std::size_t>(state.range(0));

  std::vector<std::int32_t> data(kArraySize, 1);
  std::int64_t sum = 0;

  for (auto _ : state) {
    // Keep total work per iteration constant across stride values.
    // We visit all elements each iteration, but in different access patterns.
    for (std::size_t offset = 0; offset < stride && offset < data.size(); ++offset) {
      for (std::size_t i = offset; i < data.size(); i += stride) {
        sum += data[i];
      }
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(data.size()));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(data.size() * sizeof(std::int32_t)));
  state.counters["stride"] = static_cast<double>(stride);
}

}  // namespace

BENCHMARK(BM_StrideAccess)
    ->Arg(1)
    ->Arg(4)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);
