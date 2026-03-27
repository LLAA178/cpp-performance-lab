#include <benchmark/benchmark.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {

constexpr std::size_t kOpsPerThread = 200'000;

class SpinLock {
 public:
  void lock() {
    while (flag_.test_and_set(std::memory_order_acquire)) {
    }
  }
  void unlock() { flag_.clear(std::memory_order_release); }

 private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class TicketLock {
 public:
  void lock() {
    const std::uint32_t ticket = next_.fetch_add(1, std::memory_order_relaxed);
    while (serving_.load(std::memory_order_acquire) != ticket) {
    }
  }
  void unlock() { serving_.fetch_add(1, std::memory_order_release); }

 private:
  std::atomic<std::uint32_t> next_{0};
  std::atomic<std::uint32_t> serving_{0};
};

template <class Lock>
void RunLockBenchmark(benchmark::State& state) {
  static Lock lock;
  static std::uint64_t counter = 0;
  const std::size_t work = static_cast<std::size_t>(state.range(0));

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      std::lock_guard<Lock> guard(lock);
      std::uint64_t local = counter + i;
      for (std::size_t step = 0; step < work; ++step) {
        local = (local * 1664525u) + 1013904223u;
      }
      counter = local;
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
    state.counters["work_in_lock"] = static_cast<double>(work);
  }
}

void BM_MutexLock(benchmark::State& state) { RunLockBenchmark<std::mutex>(state); }
void BM_SpinLock(benchmark::State& state) { RunLockBenchmark<SpinLock>(state); }
void BM_TicketLock(benchmark::State& state) { RunLockBenchmark<TicketLock>(state); }

}  // namespace

BENCHMARK(BM_MutexLock)
    ->Arg(1)
    ->Arg(32)
    ->Threads(1)
    ->Threads(4)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SpinLock)
    ->Arg(1)
    ->Arg(32)
    ->Threads(1)
    ->Threads(4)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TicketLock)
    ->Arg(1)
    ->Arg(32)
    ->Threads(1)
    ->Threads(4)
    ->Unit(benchmark::kMicrosecond);
