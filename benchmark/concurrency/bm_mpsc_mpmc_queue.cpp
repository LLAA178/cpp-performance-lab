#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kMsgsPerProducer = 200'000;
constexpr std::size_t kCapacity = 1u << 16;

inline void SpinHint() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield");
#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template <typename T>
class BoundedMpmcQueue {
 public:
  explicit BoundedMpmcQueue(std::size_t capacity_pow2)
      : mask_(capacity_pow2 - 1), cells_(capacity_pow2) {
    for (std::size_t i = 0; i < capacity_pow2; ++i) {
      cells_[i].seq.store(i, std::memory_order_relaxed);
    }
  }

  bool enqueue(T value) {
    std::size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = cells_[pos & mask_];
      const std::size_t seq = cell.seq.load(std::memory_order_acquire);
      const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          cell.value = value;
          cell.seq.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;
      } else {
        pos = head_.load(std::memory_order_relaxed);
      }
    }
  }

  bool dequeue(T& value) {
    std::size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
      Cell& cell = cells_[pos & mask_];
      const std::size_t seq = cell.seq.load(std::memory_order_acquire);
      const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
      if (diff == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          value = cell.value;
          cell.seq.store(pos + mask_ + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;
      } else {
        pos = tail_.load(std::memory_order_relaxed);
      }
    }
  }

 private:
  struct Cell {
    std::atomic<std::size_t> seq{0};
    T value{};
  };

  const std::size_t mask_;
  std::vector<Cell> cells_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

class MutexQueue {
 public:
  explicit MutexQueue(std::size_t) {}

  bool enqueue(std::uint64_t value) {
    std::lock_guard<std::mutex> lock(m_);
    if (q_.size() >= kCapacity) {
      return false;
    }
    q_.push(value);
    return true;
  }

  bool dequeue(std::uint64_t& value) {
    std::lock_guard<std::mutex> lock(m_);
    if (q_.empty()) {
      return false;
    }
    value = q_.front();
    q_.pop();
    return true;
  }

 private:
  std::mutex m_;
  std::queue<std::uint64_t> q_;
};

template <class Queue>
void RunQueueBenchmark(benchmark::State& state, std::size_t producers, std::size_t consumers) {
  double total_seconds = 0.0;
  const std::size_t total_msgs = producers * kMsgsPerProducer;

  for (auto _ : state) {
    state.PauseTiming();
    Queue q(kCapacity);
    std::atomic<bool> start{false};
    std::atomic<std::size_t> consumed{0};
    std::atomic<std::uint64_t> checksum{0};
    std::vector<std::thread> threads;
    threads.reserve(producers + consumers);

    for (std::size_t p = 0; p < producers; ++p) {
      threads.emplace_back([&, p]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::size_t i = 0; i < kMsgsPerProducer; ++i) {
          const std::uint64_t value = (static_cast<std::uint64_t>(p) << 32) | i;
          while (!q.enqueue(value)) {
            SpinHint();
          }
        }
      });
    }

    for (std::size_t c = 0; c < consumers; ++c) {
      threads.emplace_back([&]() {
        while (!start.load(std::memory_order_acquire)) {
        }
        std::uint64_t value = 0;
        for (;;) {
          const std::size_t done = consumed.load(std::memory_order_relaxed);
          if (done >= total_msgs) {
            break;
          }
          if (q.dequeue(value)) {
            checksum.fetch_add(value, std::memory_order_relaxed);
            consumed.fetch_add(1, std::memory_order_relaxed);
          } else {
            SpinHint();
          }
        }
      });
    }

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) {
      thread.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    benchmark::DoNotOptimize(checksum.load(std::memory_order_relaxed));
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * static_cast<double>(total_msgs) / total_seconds;
}

void BM_MutexQueueMpsc(benchmark::State& state) {
  RunQueueBenchmark<MutexQueue>(state, 4, 1);
}

void BM_MpmcQueueMpsc(benchmark::State& state) {
  RunQueueBenchmark<BoundedMpmcQueue<std::uint64_t>>(state, 4, 1);
}

void BM_MutexQueueMpmc(benchmark::State& state) {
  RunQueueBenchmark<MutexQueue>(state, 4, 4);
}

void BM_MpmcQueueMpmc(benchmark::State& state) {
  RunQueueBenchmark<BoundedMpmcQueue<std::uint64_t>>(state, 4, 4);
}

}  // namespace

BENCHMARK(BM_MutexQueueMpsc)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MpmcQueueMpsc)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MutexQueueMpmc)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MpmcQueueMpmc)->Unit(benchmark::kMillisecond);
