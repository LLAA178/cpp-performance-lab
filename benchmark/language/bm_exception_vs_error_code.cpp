#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kValueCount = 1 << 20;

std::vector<std::uint32_t> MakeValues(std::uint32_t fail_every) {
  std::vector<std::uint32_t> values(kValueCount);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = (fail_every != 0 && (i % fail_every) == 0) ? 0u : static_cast<std::uint32_t>(i + 1);
  }
  return values;
}

const std::vector<std::uint32_t>& GetRareFailValues() {
  static const std::vector<std::uint32_t> values = MakeValues(4096);
  return values;
}

const std::vector<std::uint32_t>& GetNoFailValues() {
  static const std::vector<std::uint32_t> values = MakeValues(0);
  return values;
}

std::uint64_t ParseOrThrow(std::uint32_t value) {
  if (value == 0u) {
    throw std::runtime_error("bad value");
  }
  return static_cast<std::uint64_t>(value) * 3u;
}

std::optional<std::uint64_t> ParseOrOptional(std::uint32_t value) {
  if (value == 0u) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(value) * 3u;
}

void BM_ErrorCodeNoFail(benchmark::State& state) {
  const auto& values = GetNoFailValues();
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::uint32_t value : values) {
      const auto parsed = ParseOrOptional(value);
      checksum += *parsed;
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

void BM_ExceptionNoFail(benchmark::State& state) {
  const auto& values = GetNoFailValues();
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::uint32_t value : values) {
      checksum += ParseOrThrow(value);
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

void BM_ErrorCodeRareFail(benchmark::State& state) {
  const auto& values = GetRareFailValues();
  std::uint64_t checksum = 0;
  std::size_t errors = 0;
  for (auto _ : state) {
    for (std::uint32_t value : values) {
      const auto parsed = ParseOrOptional(value);
      if (parsed.has_value()) {
        checksum += *parsed;
      } else {
        ++errors;
      }
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.counters["errors"] = static_cast<double>(errors / std::max<std::size_t>(1, state.iterations()));
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

void BM_ExceptionRareFail(benchmark::State& state) {
  const auto& values = GetRareFailValues();
  std::uint64_t checksum = 0;
  std::size_t errors = 0;
  for (auto _ : state) {
    for (std::uint32_t value : values) {
      try {
        checksum += ParseOrThrow(value);
      } catch (const std::runtime_error&) {
        ++errors;
      }
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(checksum);
  state.counters["errors"] = static_cast<double>(errors / std::max<std::size_t>(1, state.iterations()));
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(values.size()));
}

}  // namespace

BENCHMARK(BM_ErrorCodeNoFail)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ExceptionNoFail)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ErrorCodeRareFail)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ExceptionRareFail)->Unit(benchmark::kMillisecond);
