#include <benchmark/benchmark.h>

#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr std::size_t kBytes = 64 * 1024 * 1024;
constexpr std::size_t kPageBytes = 4096;

std::uint64_t TouchPages(std::uint8_t* bytes) {
  std::uint64_t checksum = 0;
  for (std::size_t offset = 0; offset < kBytes; offset += kPageBytes) {
    bytes[offset] = static_cast<std::uint8_t>((offset / kPageBytes) & 0xFFu);
    checksum += bytes[offset];
  }
  return checksum;
}

void BM_FirstTouchMapped(benchmark::State& state) {
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    void* region = ::mmap(nullptr, kBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }
    checksum += TouchPages(static_cast<std::uint8_t*>(region));
    ::munmap(region, kBytes);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kBytes));
}

void BM_PrefaultedMapped(benchmark::State& state) {
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    void* region = ::mmap(nullptr, kBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }
    auto* bytes = static_cast<std::uint8_t*>(region);
    state.PauseTiming();
    std::memset(bytes, 0, kBytes);
    state.ResumeTiming();
    checksum += TouchPages(bytes);
    ::munmap(region, kBytes);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kBytes));
}

void BM_MlockMapped(benchmark::State& state) {
  std::uint64_t checksum = 0;
  std::size_t lock_successes = 0;
  for (auto _ : state) {
    void* region = ::mmap(nullptr, kBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }
    auto* bytes = static_cast<std::uint8_t*>(region);
    state.PauseTiming();
    const bool locked = (::mlock(region, kBytes) == 0);
    lock_successes += locked ? 1u : 0u;
    std::memset(bytes, 0, kBytes);
    state.ResumeTiming();
    checksum += TouchPages(bytes);
    state.PauseTiming();
    if (locked) {
      ::munlock(region, kBytes);
    }
    state.ResumeTiming();
    ::munmap(region, kBytes);
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.counters["mlock_ok"] =
      static_cast<double>(lock_successes) / std::max<std::size_t>(1, state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kBytes));
}

}  // namespace

BENCHMARK(BM_FirstTouchMapped)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PrefaultedMapped)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MlockMapped)->Unit(benchmark::kMillisecond);
