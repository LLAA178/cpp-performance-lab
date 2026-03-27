#include <benchmark/benchmark.h>

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

std::vector<float> MakeValues(float bias) {
  std::vector<float> values(kElementCount);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<float>(((i & 255u) * 0.25f) + bias);
  }
  return values;
}

const std::vector<float>& GetBaseA() {
  static const std::vector<float> values = MakeValues(0.5f);
  return values;
}

const std::vector<float>& GetBaseB() {
  static const std::vector<float> values = MakeValues(1.5f);
  return values;
}

BM_NOINLINE void ScaleAddPotentialAlias(float* out,
                                        const float* in1,
                                        const float* in2,
                                        std::size_t n,
                                        float alpha) {
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = in1[i] + (alpha * in2[i]);
  }
}

#if defined(__clang__) || defined(__GNUC__)
BM_NOINLINE void ScaleAddNoAlias(float* __restrict out,
                                 const float* __restrict in1,
                                 const float* __restrict in2,
                                 std::size_t n,
                                 float alpha) {
#else
BM_NOINLINE void ScaleAddNoAlias(float* out,
                                 const float* in1,
                                 const float* in2,
                                 std::size_t n,
                                 float alpha) {
#endif
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = in1[i] + (alpha * in2[i]);
  }
}

void BM_PotentialAlias(benchmark::State& state) {
  std::vector<float> out = GetBaseA();
  std::vector<float> in1 = GetBaseA();
  std::vector<float> in2 = GetBaseB();
  for (auto _ : state) {
    ScaleAddPotentialAlias(out.data(), in1.data(), in2.data(), out.size(), 1.25f);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(out.data());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(out.size()));
}

void BM_NoAliasRestrict(benchmark::State& state) {
  std::vector<float> out = GetBaseA();
  std::vector<float> in1 = GetBaseA();
  std::vector<float> in2 = GetBaseB();
  for (auto _ : state) {
    ScaleAddNoAlias(out.data(), in1.data(), in2.data(), out.size(), 1.25f);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(out.data());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(out.size()));
}

void BM_OutputAliasesInput(benchmark::State& state) {
  std::vector<float> data = GetBaseA();
  std::vector<float> in2 = GetBaseB();
  for (auto _ : state) {
    ScaleAddPotentialAlias(data.data(), data.data(), in2.data(), data.size(), 1.25f);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(data.data());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}

}  // namespace

BENCHMARK(BM_PotentialAlias)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_NoAliasRestrict)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_OutputAliasesInput)->Unit(benchmark::kMillisecond);
