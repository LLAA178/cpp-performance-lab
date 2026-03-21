#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kTransferCount = 1'000'000;
constexpr std::size_t kRingCapacity = 1u << 16;

enum class BackoffKind : int {
  kYield = 0,
  kSpin = 1,
  kHybrid = 2,
};

inline void SpinHint() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm64__)
  __asm__ __volatile__("yield");
#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

inline void Backoff(BackoffKind kind, int& fail_count) {
  switch (kind) {
    case BackoffKind::kYield:
      std::this_thread::yield();
      break;
    case BackoffKind::kSpin:
      SpinHint();
      break;
    case BackoffKind::kHybrid:
      if (fail_count < 64) {
        SpinHint();
      } else {
        std::this_thread::yield();
      }
      ++fail_count;
      break;
  }
}

class SpscRingBuffer {
 public:
  explicit SpscRingBuffer(std::size_t capacity_pow2)
      : mask_(capacity_pow2 - 1), buf_(capacity_pow2) {}

  bool push(std::uint32_t v) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buf_[head] = v;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(std::uint32_t& out) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    out = buf_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return true;
  }

  std::size_t push_sequence(std::size_t want, std::uint32_t& next_value) {
    const std::size_t capacity = mask_ + 1;
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    const std::size_t used = (head + capacity - tail) & mask_;
    const std::size_t free_slots = mask_ - used;  // keep one slot empty
    const std::size_t n = std::min(want, free_slots);
    if (n == 0) {
      return 0;
    }

    std::size_t pos = head;
    for (std::size_t i = 0; i < n; ++i) {
      buf_[pos] = next_value++;
      pos = (pos + 1) & mask_;
    }
    head_.store(pos, std::memory_order_release);
    return n;
  }

  std::size_t pop_sum(std::size_t want, std::uint64_t& checksum) {
    const std::size_t capacity = mask_ + 1;
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t avail = (head + capacity - tail) & mask_;
    const std::size_t n = std::min(want, avail);
    if (n == 0) {
      return 0;
    }

    std::size_t pos = tail;
    for (std::size_t i = 0; i < n; ++i) {
      checksum += buf_[pos];
      pos = (pos + 1) & mask_;
    }
    tail_.store(pos, std::memory_order_release);
    return n;
  }

 private:
  const std::size_t mask_;
  std::vector<std::uint32_t> buf_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

void BM_QueueMutexTransfer(benchmark::State& state) {
  const std::size_t batch = static_cast<std::size_t>(state.range(0));
  const auto backoff_kind = static_cast<BackoffKind>(state.range(1));
  double total_seconds = 0.0;
  for (auto _ : state) {
    state.PauseTiming();
    std::queue<std::uint32_t> q;
    std::mutex m;
    std::atomic<bool> done{false};
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      while (!start.load(std::memory_order_acquire)) {}
      std::uint32_t i = 0;
      while (i < kTransferCount) {
        std::lock_guard<std::mutex> lock(m);
        const std::size_t n = std::min<std::size_t>(batch, kTransferCount - i);
        for (std::size_t b = 0; b < n; ++b) {
          q.push(i++);
        }
      }
      done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
      while (!start.load(std::memory_order_acquire)) {}
      std::size_t consumed = 0;
      int fail_count = 0;
      while (consumed < kTransferCount) {
        bool got = false;
        std::uint32_t v = 0;
        {
          std::lock_guard<std::mutex> lock(m);
          if (!q.empty()) {
            v = q.front();
            q.pop();
            got = true;
          }
        }
        if (got) {
          fail_count = 0;
          checksum += v;
          ++consumed;
        } else if (done.load(std::memory_order_acquire)) {
          Backoff(backoff_kind, fail_count);
        }
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
    benchmark::DoNotOptimize(checksum);
  }

  const double total_ops = static_cast<double>(state.iterations()) * kTransferCount;
  state.counters["ops_per_sec"] = total_ops / total_seconds;
}

void BM_QueueSpscRingTransfer(benchmark::State& state) {
  const std::size_t batch = static_cast<std::size_t>(state.range(0));
  const auto backoff_kind = static_cast<BackoffKind>(state.range(1));
  double total_seconds = 0.0;
  for (auto _ : state) {
    state.PauseTiming();
    SpscRingBuffer q(kRingCapacity);
    std::uint64_t checksum = 0;
    std::atomic<bool> start{false};

    std::thread producer([&]() {
      while (!start.load(std::memory_order_acquire)) {}
      std::uint32_t next_value = 0;
      std::size_t produced = 0;
      int fail_count = 0;
      while (produced < kTransferCount) {
        const std::size_t want = std::min<std::size_t>(batch, kTransferCount - produced);
        const std::size_t pushed = q.push_sequence(want, next_value);
        if (pushed == 0) {
          Backoff(backoff_kind, fail_count);
        } else {
          fail_count = 0;
          produced += pushed;
        }
      }
    });

    std::thread consumer([&]() {
      while (!start.load(std::memory_order_acquire)) {}
      std::size_t consumed = 0;
      int fail_count = 0;
      while (consumed < kTransferCount) {
        const std::size_t want = std::min<std::size_t>(batch, kTransferCount - consumed);
        const std::size_t popped = q.pop_sum(want, checksum);
        if (popped > 0) {
          fail_count = 0;
          consumed += popped;
        } else {
          Backoff(backoff_kind, fail_count);
        }
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
    benchmark::DoNotOptimize(checksum);
  }

  const double total_ops = static_cast<double>(state.iterations()) * kTransferCount;
  state.counters["ops_per_sec"] = total_ops / total_seconds;
}

}  // namespace

BENCHMARK(BM_QueueMutexTransfer)
    ->ArgsProduct({{1, 8, 64}, {0, 1, 2}})
    ->ArgNames({"batch", "backoff"})
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_QueueSpscRingTransfer)
    ->ArgsProduct({{1, 8, 64}, {0, 1, 2}})
    ->ArgNames({"batch", "backoff"})
    ->Unit(benchmark::kMillisecond);
