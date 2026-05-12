#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

constexpr std::size_t kWords = 32;
constexpr std::size_t kCallsPerIteration = 1 << 15;

#if defined(__clang__) || defined(__GNUC__)
#define BM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define BM_NOINLINE __declspec(noinline)
#else
#define BM_NOINLINE
#endif

struct ReturnStats {
  std::uint64_t copies = 0;
  std::uint64_t moves = 0;
};

ReturnStats g_return_stats;

struct Payload {
  std::array<std::uint64_t, kWords> words{};

  explicit Payload(std::uint64_t seed) {
    std::uint64_t x = seed + 0x9e3779b97f4a7c15ull;
    for (std::size_t i = 0; i < words.size(); ++i) {
      x ^= x >> 12;
      x ^= x << 25;
      x ^= x >> 27;
      words[i] = x * 0x2545f4914f6cdd1dull;
    }
  }

  Payload(const Payload& other) : words(other.words) { ++g_return_stats.copies; }

  Payload(Payload&& other) noexcept : words(other.words) { ++g_return_stats.moves; }

  Payload& operator=(const Payload&) = delete;
  Payload& operator=(Payload&&) = delete;

  std::uint64_t Sample() const {
    return words[0] ^ words[7] ^ words[15] ^ words[23] ^ words[31];
  }
};

BM_NOINLINE Payload MakePrvalue(std::uint64_t seed) {
  return Payload(seed);
}

BM_NOINLINE Payload MakeNamedNrvo(std::uint64_t seed) {
  Payload value(seed);
  return value;
}

BM_NOINLINE Payload MakeMovedLocal(std::uint64_t seed) {
  Payload value(seed);
  return std::move(value);
}

BM_NOINLINE Payload MakeTwoNamedLocals(std::uint64_t seed) {
  Payload left(seed);
  Payload right(seed ^ 0xd1b54a32d192ed03ull);
  if ((seed & 1u) == 0u) {
    return left;
  }
  return right;
}

template <Payload (*Factory)(std::uint64_t)>
void RunReturnBenchmark(benchmark::State& state) {
  g_return_stats = {};
  std::uint64_t checksum = 0;
  std::uint64_t seed = 1;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kCallsPerIteration; ++i) {
      Payload value = Factory(seed++);
      checksum ^= value.Sample();
      benchmark::DoNotOptimize(value);
    }
    benchmark::ClobberMemory();
  }

  const double calls = static_cast<double>(state.iterations()) *
                       static_cast<double>(kCallsPerIteration);
  benchmark::DoNotOptimize(checksum);
  state.counters["copies/op"] = g_return_stats.copies / calls;
  state.counters["moves/op"] = g_return_stats.moves / calls;
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCallsPerIteration));
}

void BM_ReturnPrvalue(benchmark::State& state) {
  RunReturnBenchmark<&MakePrvalue>(state);
}

void BM_ReturnNamedNrvo(benchmark::State& state) {
  RunReturnBenchmark<&MakeNamedNrvo>(state);
}

void BM_ReturnMovedLocal(benchmark::State& state) {
  RunReturnBenchmark<&MakeMovedLocal>(state);
}

void BM_ReturnTwoNamedLocals(benchmark::State& state) {
  RunReturnBenchmark<&MakeTwoNamedLocals>(state);
}

#undef BM_NOINLINE

}  // namespace

BENCHMARK(BM_ReturnPrvalue)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReturnNamedNrvo)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReturnMovedLocal)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReturnTwoNamedLocals)->Unit(benchmark::kMillisecond);
