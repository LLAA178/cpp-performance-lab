#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <vector>

namespace {

constexpr std::size_t kObjectCount = 100'000;

struct SizeClass {
  std::size_t bytes;
  std::size_t align;
};

constexpr std::array<SizeClass, 4> kClasses{{
    {32, alignof(std::max_align_t)},
    {64, alignof(std::max_align_t)},
    {128, alignof(std::max_align_t)},
    {256, alignof(std::max_align_t)},
}};

struct Allocation {
  void* ptr = nullptr;
  std::size_t bytes = 0;
  std::size_t align = alignof(std::max_align_t);
};

std::vector<Allocation> MakePlan() {
  std::vector<Allocation> plan(kObjectCount);
  for (std::size_t i = 0; i < plan.size(); ++i) {
    const auto& cls = kClasses[i & (kClasses.size() - 1)];
    plan[i].bytes = cls.bytes;
    plan[i].align = cls.align;
  }
  return plan;
}

const std::vector<Allocation>& GetPlan() {
  static const std::vector<Allocation> plan = MakePlan();
  return plan;
}

template <class AllocFn, class FreeFn>
void RunMixedAllocatorBenchmark(benchmark::State& state, AllocFn alloc_fn, FreeFn free_fn) {
  const auto& plan = GetPlan();
  double total_seconds = 0.0;

  for (auto _ : state) {
    std::vector<Allocation> allocs = plan;
    const auto t0 = std::chrono::steady_clock::now();
    for (Allocation& item : allocs) {
      item.ptr = alloc_fn(item.bytes, item.align);
      static_cast<std::uint8_t*>(item.ptr)[0] = static_cast<std::uint8_t>(item.bytes);
    }
    for (Allocation& item : allocs) {
      benchmark::DoNotOptimize(static_cast<std::uint8_t*>(item.ptr)[0]);
      free_fn(item.ptr, item.bytes, item.align);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * static_cast<double>(plan.size()) / total_seconds;
}

void BM_MixedNewDelete(benchmark::State& state) {
  RunMixedAllocatorBenchmark(
      state,
      [](std::size_t bytes, std::size_t align) { return ::operator new(bytes, std::align_val_t(align)); },
      [](void* ptr, std::size_t bytes, std::size_t align) {
        ::operator delete(ptr, bytes, std::align_val_t(align));
      });
}

void BM_MixedMallocFree(benchmark::State& state) {
  RunMixedAllocatorBenchmark(
      state,
      [](std::size_t bytes, std::size_t) { return std::malloc(bytes); },
      [](void* ptr, std::size_t, std::size_t) { std::free(ptr); });
}

void BM_MixedPmrUnsyncPool(benchmark::State& state) {
  const auto& plan = GetPlan();
  double total_seconds = 0.0;

  for (auto _ : state) {
    std::pmr::unsynchronized_pool_resource resource;
    std::vector<Allocation> allocs = plan;
    const auto t0 = std::chrono::steady_clock::now();
    for (Allocation& item : allocs) {
      item.ptr = resource.allocate(item.bytes, item.align);
      static_cast<std::uint8_t*>(item.ptr)[0] = static_cast<std::uint8_t>(item.bytes);
    }
    for (Allocation& item : allocs) {
      benchmark::DoNotOptimize(static_cast<std::uint8_t*>(item.ptr)[0]);
      resource.deallocate(item.ptr, item.bytes, item.align);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * static_cast<double>(plan.size()) / total_seconds;
}

}  // namespace

BENCHMARK(BM_MixedNewDelete)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MixedMallocFree)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MixedPmrUnsyncPool)->Unit(benchmark::kMillisecond);
