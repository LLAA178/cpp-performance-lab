#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

namespace {

constexpr std::size_t kRounds = 200'000;

inline void SpinHint() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield");
#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

void BM_SpinHandoff(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<int> turn{0};
    std::uint64_t checksum = 0;
    std::thread worker([&]() {
      for (std::size_t i = 0; i < kRounds; ++i) {
        while (turn.load(std::memory_order_acquire) != 1) {
          SpinHint();
        }
        checksum += i;
        turn.store(0, std::memory_order_release);
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kRounds; ++i) {
      while (turn.load(std::memory_order_acquire) != 0) {
        SpinHint();
      }
      checksum += i;
      turn.store(1, std::memory_order_release);
    }
    while (turn.load(std::memory_order_acquire) != 0) {
      SpinHint();
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();
    worker.join();
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }
  state.counters["handoffs_per_sec"] =
      static_cast<double>(state.iterations()) * (2.0 * kRounds) / total_seconds;
}

void BM_YieldHandoff(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<int> turn{0};
    std::uint64_t checksum = 0;
    std::thread worker([&]() {
      for (std::size_t i = 0; i < kRounds; ++i) {
        while (turn.load(std::memory_order_acquire) != 1) {
          std::this_thread::yield();
        }
        checksum += i;
        turn.store(0, std::memory_order_release);
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kRounds; ++i) {
      while (turn.load(std::memory_order_acquire) != 0) {
        std::this_thread::yield();
      }
      checksum += i;
      turn.store(1, std::memory_order_release);
    }
    while (turn.load(std::memory_order_acquire) != 0) {
      std::this_thread::yield();
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();
    worker.join();
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }
  state.counters["handoffs_per_sec"] =
      static_cast<double>(state.iterations()) * (2.0 * kRounds) / total_seconds;
}

void BM_CondVarHandoff(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    state.PauseTiming();
    std::mutex m;
    std::condition_variable cv;
    int turn = 0;
    std::uint64_t checksum = 0;
    std::thread worker([&]() {
      for (std::size_t i = 0; i < kRounds; ++i) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&] { return turn == 1; });
        checksum += i;
        turn = 0;
        lock.unlock();
        cv.notify_one();
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kRounds; ++i) {
      std::unique_lock<std::mutex> lock(m);
      cv.wait(lock, [&] { return turn == 0; });
      checksum += i;
      turn = 1;
      lock.unlock();
      cv.notify_one();
    }
    {
      std::unique_lock<std::mutex> lock(m);
      cv.wait(lock, [&] { return turn == 0; });
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();
    worker.join();
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }
  state.counters["handoffs_per_sec"] =
      static_cast<double>(state.iterations()) * (2.0 * kRounds) / total_seconds;
}

}  // namespace

BENCHMARK(BM_SpinHandoff)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_YieldHandoff)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_CondVarHandoff)->Unit(benchmark::kMillisecond);
