#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr std::size_t kItemCount = 1 << 20;
constexpr std::size_t kOpsPerIteration = 8;

struct Payload {
  std::uint64_t x;
  std::uint64_t y;
};

struct VirtualBase {
  virtual ~VirtualBase() = default;
  virtual std::uint64_t Apply(const Payload& p) const = 0;
};

struct VirtualAdder final : VirtualBase {
  std::uint64_t Apply(const Payload& p) const override {
    return (p.x * 3u) + (p.y * 7u);
  }
};

struct StaticAdder {
  std::uint64_t Apply(const Payload& p) const {
    return (p.x * 3u) + (p.y * 7u);
  }
};

std::vector<Payload> MakePayloads() {
  std::vector<Payload> payloads(kItemCount);
  for (std::size_t i = 0; i < payloads.size(); ++i) {
    payloads[i] = Payload{static_cast<std::uint64_t>(i + 1),
                          static_cast<std::uint64_t>((i * 17u) ^ 0x9e3779b9u)};
  }
  return payloads;
}

const std::vector<Payload>& GetPayloads() {
  static const std::vector<Payload> payloads = MakePayloads();
  return payloads;
}

void BM_TemplateDispatch(benchmark::State& state) {
  const auto& payloads = GetPayloads();
  StaticAdder op;
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const Payload& p : payloads) {
        checksum += op.Apply(p);
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(payloads.size() * kOpsPerIteration));
}

void BM_VirtualDispatch(benchmark::State& state) {
  const auto& payloads = GetPayloads();
  VirtualAdder impl;
  const VirtualBase* op = &impl;
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const Payload& p : payloads) {
        checksum += op->Apply(p);
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(payloads.size() * kOpsPerIteration));
}

std::uint64_t FunctionPointerApply(const Payload& p) {
  return (p.x * 3u) + (p.y * 7u);
}

void BM_FunctionPointerDispatch(benchmark::State& state) {
  const auto& payloads = GetPayloads();
  auto* fn = &FunctionPointerApply;
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const Payload& p : payloads) {
        checksum += fn(p);
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(payloads.size() * kOpsPerIteration));
}

}  // namespace

BENCHMARK(BM_TemplateDispatch)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_VirtualDispatch)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FunctionPointerDispatch)->Unit(benchmark::kMillisecond);
