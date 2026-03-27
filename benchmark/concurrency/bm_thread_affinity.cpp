#include <benchmark/benchmark.h>

#include <pthread.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#elif defined(__linux__)
#include <sched.h>
#endif

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kTransferCount = 1'000'000;
constexpr std::size_t kRingCapacity = 1u << 14;

class SpscRingBuffer {
 public:
  SpscRingBuffer() : buf_(kRingCapacity) {}

  bool push(std::uint32_t value) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & (kRingCapacity - 1);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buf_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(std::uint32_t& value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = buf_[tail];
    tail_.store((tail + 1) & (kRingCapacity - 1), std::memory_order_release);
    return true;
  }

 private:
  std::vector<std::uint32_t> buf_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

enum class PlacementMode {
  kDefault = 0,
  kShared = 1,
  kSplit = 2,
};

struct PlacementResult {
  bool requested = false;
  bool verified = false;
};

PlacementResult ApplyPlacement(PlacementMode mode, unsigned int slot) {
  if (mode == PlacementMode::kDefault) {
    return {};
  }

#if defined(__linux__)
  const unsigned int cpu = (mode == PlacementMode::kShared) ? 0u : slot;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (::pthread_setaffinity_np(::pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
    return {};
  }

  cpu_set_t verify_set;
  CPU_ZERO(&verify_set);
  if (::pthread_getaffinity_np(::pthread_self(), sizeof(cpu_set_t), &verify_set) != 0) {
    return {true, false};
  }
  return {true, CPU_ISSET(cpu, &verify_set) != 0};
#elif defined(__APPLE__)
  const integer_t tag = static_cast<integer_t>((mode == PlacementMode::kShared) ? 1 : (slot + 1));
  thread_affinity_policy_data_t policy;
  policy.affinity_tag = tag;
  if (thread_policy_set(pthread_mach_thread_np(::pthread_self()),
                        THREAD_AFFINITY_POLICY,
                        reinterpret_cast<thread_policy_t>(&policy),
                        THREAD_AFFINITY_POLICY_COUNT) != KERN_SUCCESS) {
    return {};
  }

  thread_affinity_policy_data_t verify_policy;
  mach_msg_type_number_t count = THREAD_AFFINITY_POLICY_COUNT;
  boolean_t get_default = FALSE;
  if (thread_policy_get(pthread_mach_thread_np(::pthread_self()),
                        THREAD_AFFINITY_POLICY,
                        reinterpret_cast<thread_policy_t>(&verify_policy),
                        &count,
                        &get_default) != KERN_SUCCESS) {
    return {true, false};
  }
  return {true, verify_policy.affinity_tag == tag};
#else
  (void)slot;
  return {};
#endif
}

void RunTransferBenchmark(benchmark::State& state, PlacementMode mode) {
  double total_seconds = 0.0;
  std::atomic<std::size_t> requests{0};
  std::atomic<std::size_t> verified{0};

  for (auto _ : state) {
    state.PauseTiming();
    SpscRingBuffer ring;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      const PlacementResult result = ApplyPlacement(mode, 0);
      requests.fetch_add(result.requested ? 1u : 0u, std::memory_order_relaxed);
      verified.fetch_add(result.verified ? 1u : 0u, std::memory_order_relaxed);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::uint32_t value = 0; value < kTransferCount; ++value) {
        while (!ring.push(value)) {
        }
      }
    });

    std::thread consumer([&]() {
      const PlacementResult result = ApplyPlacement(mode, 1);
      requests.fetch_add(result.requested ? 1u : 0u, std::memory_order_relaxed);
      verified.fetch_add(result.verified ? 1u : 0u, std::memory_order_relaxed);
      while (!start.load(std::memory_order_acquire)) {
      }
      std::uint32_t value = 0;
      for (std::size_t i = 0; i < kTransferCount; ++i) {
        while (!ring.pop(value)) {
        }
        checksum += value;
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

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * kTransferCount / total_seconds;
  state.counters["placement_requests"] =
      static_cast<double>(requests.load(std::memory_order_relaxed)) / (2.0 * state.iterations());
  state.counters["placement_verified"] =
      static_cast<double>(verified.load(std::memory_order_relaxed)) / (2.0 * state.iterations());
}

void BM_ThreadTransferDefault(benchmark::State& state) {
  RunTransferBenchmark(state, PlacementMode::kDefault);
}

void BM_ThreadTransferSharedPlacement(benchmark::State& state) {
  RunTransferBenchmark(state, PlacementMode::kShared);
}

void BM_ThreadTransferSplitPlacement(benchmark::State& state) {
  RunTransferBenchmark(state, PlacementMode::kSplit);
}

}  // namespace

BENCHMARK(BM_ThreadTransferDefault)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ThreadTransferSharedPlacement)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ThreadTransferSplitPlacement)->Unit(benchmark::kMillisecond);
