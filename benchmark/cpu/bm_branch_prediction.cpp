#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t kElementCount = 1 << 20;

#if defined(__clang__) || defined(__GNUC__)
#define BM_NOINLINE __attribute__((noinline))
#else
#define BM_NOINLINE
#endif

const std::vector<std::uint8_t>& GetAlwaysTaken() {
  static const std::vector<std::uint8_t> values(kElementCount, 1);
  return values;
}

const std::vector<std::uint8_t>& GetAlternating() {
  static const std::vector<std::uint8_t> values = [] {
    std::vector<std::uint8_t> out(kElementCount);
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = static_cast<std::uint8_t>(i & 1u);
    }
    return out;
  }();
  return values;
}

const std::vector<std::uint8_t>& GetPseudoRandom() {
  static const std::vector<std::uint8_t> values = [] {
    std::vector<std::uint8_t> out(kElementCount);
    std::uint32_t x = 0x12345678u;
    for (std::size_t i = 0; i < out.size(); ++i) {
      x = (x * 1664525u) + 1013904223u;
      out[i] = static_cast<std::uint8_t>((x >> 31) & 1u);
    }
    return out;
  }();
  return values;
}

BM_NOINLINE std::uint64_t CountBranchy(const std::uint8_t* values, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (values[i]) {
      sum += 3;
    } else {
      sum += 1;
    }
  }
  return sum;
}

BM_NOINLINE std::uint64_t CountBranchless(const std::uint8_t* values, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    sum += 1 + (static_cast<std::uint64_t>(values[i]) << 1u);
  }
  return sum;
}

template <const std::vector<std::uint8_t>& (*GetValues)(), bool kBranchless>
void RunBranchBench(benchmark::State& state) {
  const auto& values = GetValues();
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    checksum += kBranchless ? CountBranchless(values.data(), values.size())
                            : CountBranchy(values.data(), values.size());
    benchmark::DoNotOptimize(checksum);
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

void BM_BranchAlwaysTaken(benchmark::State& state) {
  RunBranchBench<GetAlwaysTaken, false>(state);
}

void BM_BranchAlternating(benchmark::State& state) {
  RunBranchBench<GetAlternating, false>(state);
}

void BM_BranchPseudoRandom(benchmark::State& state) {
  RunBranchBench<GetPseudoRandom, false>(state);
}

void BM_BranchlessPseudoRandom(benchmark::State& state) {
  RunBranchBench<GetPseudoRandom, true>(state);
}

}  // namespace

BENCHMARK(BM_BranchAlwaysTaken)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_BranchAlternating)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_BranchPseudoRandom)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_BranchlessPseudoRandom)->Unit(benchmark::kMicrosecond);
