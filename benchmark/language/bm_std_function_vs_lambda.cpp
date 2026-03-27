#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace {

constexpr std::size_t kValueCount = 1 << 20;
constexpr std::size_t kPasses = 8;

std::vector<std::uint64_t> MakeValues() {
  std::vector<std::uint64_t> values(kValueCount);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<std::uint64_t>((i * 48271u) ^ (i >> 3));
  }
  return values;
}

const std::vector<std::uint64_t>& GetValues() {
  static const std::vector<std::uint64_t> values = MakeValues();
  return values;
}

template <class Fn>
void RunCallableBenchmark(benchmark::State& state, Fn&& fn) {
  const auto& values = GetValues();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t pass = 0; pass < kPasses; ++pass) {
      for (std::uint64_t value : values) {
        checksum += fn(value);
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size() * kPasses));
}

std::uint64_t ApplyFn(std::uint64_t x) {
  return (x * 13u) ^ (x >> 7);
}

struct Functor {
  std::uint64_t operator()(std::uint64_t x) const {
    return (x * 13u) ^ (x >> 7);
  }
};

void BM_LambdaCallable(benchmark::State& state) {
  const auto lambda = [](std::uint64_t x) { return (x * 13u) ^ (x >> 7); };
  RunCallableBenchmark(state, lambda);
}

void BM_FunctorCallable(benchmark::State& state) {
  RunCallableBenchmark(state, Functor{});
}

void BM_FunctionPointerCallable(benchmark::State& state) {
  RunCallableBenchmark(state, &ApplyFn);
}

void BM_StdFunctionCallable(benchmark::State& state) {
  std::function<std::uint64_t(std::uint64_t)> fn =
      [](std::uint64_t x) { return (x * 13u) ^ (x >> 7); };
  RunCallableBenchmark(state, fn);
}

}  // namespace

BENCHMARK(BM_LambdaCallable)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FunctorCallable)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FunctionPointerCallable)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StdFunctionCallable)->Unit(benchmark::kMillisecond);
