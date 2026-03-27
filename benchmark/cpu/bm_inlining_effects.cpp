#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kElementCount = 1 << 20;

#if defined(__clang__) || defined(__GNUC__)
#define BM_NOINLINE __attribute__((noinline))
#define BM_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define BM_NOINLINE
#define BM_ALWAYS_INLINE inline
#endif

BM_ALWAYS_INLINE std::uint64_t StepInline(std::uint64_t x) {
  return (x * 1664525u) + 1013904223u;
}

BM_NOINLINE std::uint64_t StepNoInline(std::uint64_t x) {
  return (x * 1664525u) + 1013904223u;
}

using StepFn = std::uint64_t (*)(std::uint64_t);

void BM_InlineStep(benchmark::State& state) {
  std::uint64_t x = 1;
  for (auto _ : state) {
    for (std::size_t i = 0; i < kElementCount; ++i) {
      x = StepInline(x);
    }
    benchmark::DoNotOptimize(x);
  }
  benchmark::DoNotOptimize(x);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kElementCount));
}

void BM_NoInlineStep(benchmark::State& state) {
  std::uint64_t x = 1;
  for (auto _ : state) {
    for (std::size_t i = 0; i < kElementCount; ++i) {
      x = StepNoInline(x);
    }
    benchmark::DoNotOptimize(x);
  }
  benchmark::DoNotOptimize(x);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kElementCount));
}

void BM_FunctionPointerStep(benchmark::State& state) {
  std::uint64_t x = 1;
  StepFn fn = &StepNoInline;
  for (auto _ : state) {
    for (std::size_t i = 0; i < kElementCount; ++i) {
      x = fn(x);
    }
    benchmark::DoNotOptimize(x);
  }
  benchmark::DoNotOptimize(x);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kElementCount));
}

}  // namespace

BENCHMARK(BM_InlineStep)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_NoInlineStep)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FunctionPointerStep)->Unit(benchmark::kMicrosecond);
