#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <array>

namespace {

constexpr std::size_t kTransferCount = 200'000;
constexpr std::size_t kRingCapacity = 1u << 14;

template <std::size_t PayloadBytes>
struct Message {
  std::array<std::uint8_t, PayloadBytes> bytes{};
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

template <std::size_t PayloadBytes>
class SpscRingBuffer {
 public:
  SpscRingBuffer() : buf_(kRingCapacity) {}

  bool push(const Message<PayloadBytes>& value) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & (kRingCapacity - 1);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buf_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(Message<PayloadBytes>& value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = buf_[tail];
    tail_.store((tail + 1) & (kRingCapacity - 1), std::memory_order_release);
    return true;
  }

 private:
  std::vector<Message<PayloadBytes>> buf_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

template <std::size_t PayloadBytes>
void RunMutexQueueBenchmark(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    std::queue<Message<PayloadBytes>> q;
    std::mutex m;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      Message<PayloadBytes> msg;
      std::memset(msg.bytes.data(), static_cast<int>(PayloadBytes), PayloadBytes);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; ++i) {
        for (;;) {
          std::lock_guard<std::mutex> guard(m);
          if (q.size() < kRingCapacity) {
            q.push(msg);
            break;
          }
        }
      }
    });

    std::thread consumer([&]() {
      Message<PayloadBytes> msg;
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; ++i) {
        bool got = false;
        {
          std::lock_guard<std::mutex> guard(m);
          if (!q.empty()) {
            msg = q.front();
            q.pop();
            got = true;
          }
        }
        if (got) {
          checksum += msg.bytes[0];
        } else {
          --i;
          SpinHint();
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

    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount / total_seconds;
  state.counters["payload_bytes"] = static_cast<double>(PayloadBytes);
  state.counters["bytes_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount * PayloadBytes / total_seconds;
}

template <std::size_t PayloadBytes>
void RunSpscQueueBenchmark(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    SpscRingBuffer<PayloadBytes> q;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      Message<PayloadBytes> msg;
      std::memset(msg.bytes.data(), static_cast<int>(PayloadBytes), PayloadBytes);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; ++i) {
        while (!q.push(msg)) {
          SpinHint();
        }
      }
    });

    std::thread consumer([&]() {
      Message<PayloadBytes> msg;
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; ++i) {
        while (!q.pop(msg)) {
          SpinHint();
        }
        checksum += msg.bytes[0];
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount / total_seconds;
  state.counters["payload_bytes"] = static_cast<double>(PayloadBytes);
  state.counters["bytes_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount * PayloadBytes / total_seconds;
}

template <std::size_t PayloadBytes, std::size_t BatchSize>
void RunMutexQueueBatchBenchmark(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    std::queue<Message<PayloadBytes>> q;
    std::mutex m;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      Message<PayloadBytes> msg;
      std::memset(msg.bytes.data(), static_cast<int>(PayloadBytes), PayloadBytes);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; i += BatchSize) {
        const std::size_t batch = std::min<std::size_t>(BatchSize, kTransferCount - i);
        for (;;) {
          std::lock_guard<std::mutex> guard(m);
          if (q.size() + batch <= kRingCapacity) {
            for (std::size_t j = 0; j < batch; ++j) {
              q.push(msg);
            }
            break;
          }
        }
      }
    });

    std::thread consumer([&]() {
      Message<PayloadBytes> msg;
      while (!start.load(std::memory_order_acquire)) {
      }
      std::size_t consumed = 0;
      while (consumed < kTransferCount) {
        const std::size_t batch = std::min<std::size_t>(BatchSize, kTransferCount - consumed);
        std::size_t got = 0;
        {
          std::lock_guard<std::mutex> guard(m);
          while (got < batch && !q.empty()) {
            msg = q.front();
            q.pop();
            checksum += msg.bytes[0];
            ++got;
          }
        }
        if (got == 0) {
          SpinHint();
        } else {
          consumed += got;
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

    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount / total_seconds;
  state.counters["bytes_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount * PayloadBytes / total_seconds;
  state.counters["payload_bytes"] = static_cast<double>(PayloadBytes);
  state.counters["batch_msgs"] = static_cast<double>(BatchSize);
}

template <std::size_t PayloadBytes, std::size_t BatchSize>
void RunSpscQueueBatchBenchmark(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    SpscRingBuffer<PayloadBytes> q;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      Message<PayloadBytes> msg;
      std::memset(msg.bytes.data(), static_cast<int>(PayloadBytes), PayloadBytes);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; i += BatchSize) {
        const std::size_t batch = std::min<std::size_t>(BatchSize, kTransferCount - i);
        for (std::size_t j = 0; j < batch; ++j) {
          while (!q.push(msg)) {
            SpinHint();
          }
        }
      }
    });

    std::thread consumer([&]() {
      Message<PayloadBytes> msg;
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kTransferCount; i += BatchSize) {
        const std::size_t batch = std::min<std::size_t>(BatchSize, kTransferCount - i);
        for (std::size_t j = 0; j < batch; ++j) {
          while (!q.pop(msg)) {
            SpinHint();
          }
          checksum += msg.bytes[0];
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

    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount / total_seconds;
  state.counters["bytes_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount * PayloadBytes / total_seconds;
  state.counters["payload_bytes"] = static_cast<double>(PayloadBytes);
  state.counters["batch_msgs"] = static_cast<double>(BatchSize);
}

void BM_MutexQueuePayload64(benchmark::State& state) { RunMutexQueueBenchmark<64>(state); }
void BM_MutexQueuePayload256(benchmark::State& state) { RunMutexQueueBenchmark<256>(state); }
void BM_MutexQueuePayload1024(benchmark::State& state) { RunMutexQueueBenchmark<1024>(state); }
void BM_SpscQueuePayload64(benchmark::State& state) { RunSpscQueueBenchmark<64>(state); }
void BM_SpscQueuePayload256(benchmark::State& state) { RunSpscQueueBenchmark<256>(state); }
void BM_SpscQueuePayload1024(benchmark::State& state) { RunSpscQueueBenchmark<1024>(state); }
void BM_MutexQueuePayload256Batch8(benchmark::State& state) {
  RunMutexQueueBatchBenchmark<256, 8>(state);
}
void BM_SpscQueuePayload256Batch8(benchmark::State& state) {
  RunSpscQueueBatchBenchmark<256, 8>(state);
}

}  // namespace

BENCHMARK(BM_MutexQueuePayload64)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MutexQueuePayload256)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MutexQueuePayload1024)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SpscQueuePayload64)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SpscQueuePayload256)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SpscQueuePayload1024)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MutexQueuePayload256Batch8)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SpscQueuePayload256Batch8)->Unit(benchmark::kMillisecond);
