#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <vector>

namespace {

constexpr std::size_t kItemCount = 1 << 20;

std::vector<std::uint64_t> MakeValues() {
  std::vector<std::uint64_t> values(kItemCount);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<std::uint64_t>((i * 48271u) ^ 0x9e3779b97f4a7c15ULL);
  }
  return values;
}

const std::vector<std::uint64_t>& GetValues() {
  static const std::vector<std::uint64_t> values = MakeValues();
  return values;
}

void BM_VectorScan(benchmark::State& state) {
  const auto& values = GetValues();
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::uint64_t v : values) {
      checksum += v;
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

void BM_DequeScan(benchmark::State& state) {
  const auto& values = GetValues();
  std::deque<std::uint64_t> dq(values.begin(), values.end());
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::uint64_t v : dq) {
      checksum += v;
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(dq.size()));
}

void BM_ListScan(benchmark::State& state) {
  const auto& values = GetValues();
  std::list<std::uint64_t> lst(values.begin(), values.end());
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::uint64_t v : lst) {
      checksum += v;
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(lst.size()));
}

}  // namespace

BENCHMARK(BM_VectorScan)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_DequeScan)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ListScan)->Unit(benchmark::kMillisecond);
