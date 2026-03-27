#include <benchmark/benchmark.h>

#include <sys/time.h>
#include <time.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kCallsPerIteration = 1'000'000;

void BM_SteadyClockNow(benchmark::State& state) {
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::size_t i = 0; i < kCallsPerIteration; ++i) {
      checksum += static_cast<std::uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count());
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCallsPerIteration));
}

void BM_SystemClockNow(benchmark::State& state) {
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::size_t i = 0; i < kCallsPerIteration; ++i) {
      checksum += static_cast<std::uint64_t>(
          std::chrono::system_clock::now().time_since_epoch().count());
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCallsPerIteration));
}

void BM_ClockGettimeMonotonic(benchmark::State& state) {
  std::uint64_t checksum = 0;
  timespec ts{};
  for (auto _ : state) {
    for (std::size_t i = 0; i < kCallsPerIteration; ++i) {
#if defined(CLOCK_MONOTONIC_RAW)
      ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
      ::clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
      checksum += static_cast<std::uint64_t>(ts.tv_nsec);
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCallsPerIteration));
}

void BM_Gettimeofday(benchmark::State& state) {
  std::uint64_t checksum = 0;
  timeval tv{};
  for (auto _ : state) {
    for (std::size_t i = 0; i < kCallsPerIteration; ++i) {
      ::gettimeofday(&tv, nullptr);
      checksum += static_cast<std::uint64_t>(tv.tv_usec);
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kCallsPerIteration));
}

}  // namespace

BENCHMARK(BM_SteadyClockNow)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SystemClockNow)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ClockGettimeMonotonic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Gettimeofday)->Unit(benchmark::kMillisecond);
