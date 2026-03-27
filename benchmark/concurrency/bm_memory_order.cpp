#include <benchmark/benchmark.h>

#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kOpsPerThread = 500'000;
constexpr std::size_t kPingPongRounds = 200'000;
constexpr std::size_t kPublishRounds = 200'000;
constexpr std::size_t kRingRounds = 500'000;
constexpr std::size_t kRingCapacity = 1u << 12;
constexpr std::size_t kLitmusRounds = 100'000;

void BM_FetchAddRelaxed(benchmark::State& state) {
  static std::atomic<std::uint64_t> counter{0};
  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      counter.fetch_add(1, std::memory_order_relaxed);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
  }
}

void BM_FetchAddAcqRel(benchmark::State& state) {
  static std::atomic<std::uint64_t> counter{0};
  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      counter.fetch_add(1, std::memory_order_acq_rel);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
  }
}

void BM_FetchAddSeqCst(benchmark::State& state) {
  static std::atomic<std::uint64_t> counter{0};
  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerThread; ++i) {
      counter.fetch_add(1, std::memory_order_seq_cst);
    }
    benchmark::ClobberMemory();
  }

  if (state.thread_index() == 0) {
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(kOpsPerThread * state.threads()));
  }
}

template <std::memory_order StoreOrder, std::memory_order LoadOrder>
void RunFlagHandoff(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<int> turn{0};
    std::uint64_t checksum = 0;

    std::thread worker([&]() {
      for (std::size_t round = 0; round < kPingPongRounds; ++round) {
        while (turn.load(LoadOrder) != 1) {
        }
        checksum += round;
        turn.store(0, StoreOrder);
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < kPingPongRounds; ++round) {
      while (turn.load(LoadOrder) != 0) {
      }
      checksum += round;
      turn.store(1, StoreOrder);
    }
    while (turn.load(LoadOrder) != 0) {
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    worker.join();
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["handoffs_per_sec"] =
      static_cast<double>(state.iterations()) * (2.0 * kPingPongRounds) / total_seconds;
}

void BM_FlagHandoffAcqRel(benchmark::State& state) {
  RunFlagHandoff<std::memory_order_release, std::memory_order_acquire>(state);
}

void BM_FlagHandoffSeqCst(benchmark::State& state) {
  RunFlagHandoff<std::memory_order_seq_cst, std::memory_order_seq_cst>(state);
}

template <std::memory_order FlagStoreOrder, std::memory_order FlagLoadOrder>
void RunPublishConsume(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<std::uint64_t> payload{0};
    std::atomic<std::uint32_t> ready{0};
    std::uint64_t checksum = 0;

    std::thread consumer([&]() {
      for (std::size_t round = 0; round < kPublishRounds; ++round) {
        while (ready.load(FlagLoadOrder) != 1u) {
        }
        checksum += payload.load(std::memory_order_relaxed);
        ready.store(0u, std::memory_order_relaxed);
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < kPublishRounds; ++round) {
      while (ready.load(std::memory_order_relaxed) != 0u) {
      }
      payload.store(static_cast<std::uint64_t>(round + 1), std::memory_order_relaxed);
      ready.store(1u, FlagStoreOrder);
    }
    consumer.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["publishes_per_sec"] =
      static_cast<double>(state.iterations()) * kPublishRounds / total_seconds;
}

void BM_PublishConsumeReleaseAcquire(benchmark::State& state) {
  RunPublishConsume<std::memory_order_release, std::memory_order_acquire>(state);
}

void BM_PublishConsumeSeqCst(benchmark::State& state) {
  RunPublishConsume<std::memory_order_seq_cst, std::memory_order_seq_cst>(state);
}

template <std::memory_order MetaLoadOrder, std::memory_order MetaStoreOrder>
class OrderedSpscRing {
 public:
  OrderedSpscRing() : buf_(kRingCapacity) {}

  bool push(std::uint64_t value) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & (kRingCapacity - 1);
    if (next == tail_.load(MetaLoadOrder)) {
      return false;
    }
    buf_[head] = value;
    head_.store(next, MetaStoreOrder);
    return true;
  }

  bool pop(std::uint64_t& value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(MetaLoadOrder)) {
      return false;
    }
    value = buf_[tail];
    tail_.store((tail + 1) & (kRingCapacity - 1), MetaStoreOrder);
    return true;
  }

 private:
  std::vector<std::uint64_t> buf_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

template <std::memory_order MetaLoadOrder, std::memory_order MetaStoreOrder>
void RunRingTransfer(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    OrderedSpscRing<MetaLoadOrder, MetaStoreOrder> ring;
    std::atomic<bool> start{false};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::uint64_t value = 0; value < kRingRounds; ++value) {
        while (!ring.push(value)) {
        }
      }
    });

    std::thread consumer([&]() {
      while (!start.load(std::memory_order_acquire)) {
      }
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < kRingRounds; ++i) {
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

  state.counters["ring_ops_per_sec"] =
      static_cast<double>(state.iterations()) * kRingRounds / total_seconds;
}

void BM_RingTransferAcqRel(benchmark::State& state) {
  RunRingTransfer<std::memory_order_acquire, std::memory_order_release>(state);
}

void BM_RingTransferSeqCst(benchmark::State& state) {
  RunRingTransfer<std::memory_order_seq_cst, std::memory_order_seq_cst>(state);
}

template <std::memory_order FlagStoreOrder, std::memory_order FlagLoadOrder>
void RunMessagePassingLitmus(benchmark::State& state, const char* counter_prefix) {
  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<int> payload{0};
    std::atomic<int> ready{0};
    std::atomic<int> observed{-1};
    std::barrier sync_point(3);
    std::uint64_t bad_reads = 0;

    std::thread producer([&]() {
      for (std::size_t round = 0; round < kLitmusRounds; ++round) {
        sync_point.arrive_and_wait();
        payload.store(1, std::memory_order_relaxed);
        ready.store(1, FlagStoreOrder);
        sync_point.arrive_and_wait();
      }
    });

    std::thread consumer([&]() {
      for (std::size_t round = 0; round < kLitmusRounds; ++round) {
        sync_point.arrive_and_wait();
        while (ready.load(FlagLoadOrder) != 1) {
        }
        observed.store(payload.load(std::memory_order_relaxed), std::memory_order_relaxed);
        sync_point.arrive_and_wait();
      }
    });

    state.ResumeTiming();
    for (std::size_t round = 0; round < kLitmusRounds; ++round) {
      payload.store(0, std::memory_order_relaxed);
      ready.store(0, std::memory_order_relaxed);
      observed.store(-1, std::memory_order_relaxed);
      sync_point.arrive_and_wait();
      sync_point.arrive_and_wait();
      if (observed.load(std::memory_order_relaxed) != 1) {
        ++bad_reads;
      }
    }
    state.PauseTiming();

    producer.join();
    consumer.join();
    state.counters[std::string(counter_prefix) + "_bad_reads"] =
        static_cast<double>(bad_reads);
    state.counters[std::string(counter_prefix) + "_bad_rate"] =
        static_cast<double>(bad_reads) / static_cast<double>(kLitmusRounds);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kLitmusRounds));
}

void BM_MessagePassingRelaxed(benchmark::State& state) {
  RunMessagePassingLitmus<std::memory_order_relaxed, std::memory_order_relaxed>(
      state, "relaxed");
}

void BM_MessagePassingReleaseAcquire(benchmark::State& state) {
  RunMessagePassingLitmus<std::memory_order_release, std::memory_order_acquire>(
      state, "release_acquire");
}

template <std::memory_order StoreOrder, std::memory_order LoadOrder>
void RunStoreBufferingLitmus(benchmark::State& state, const char* counter_prefix) {
  for (auto _ : state) {
    state.PauseTiming();
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    std::atomic<int> r1{-1};
    std::atomic<int> r2{-1};
    std::barrier sync_point(3);
    std::uint64_t both_zero = 0;

    std::thread t1([&]() {
      for (std::size_t round = 0; round < kLitmusRounds; ++round) {
        sync_point.arrive_and_wait();
        x.store(1, StoreOrder);
        r1.store(y.load(LoadOrder), std::memory_order_relaxed);
        sync_point.arrive_and_wait();
      }
    });

    std::thread t2([&]() {
      for (std::size_t round = 0; round < kLitmusRounds; ++round) {
        sync_point.arrive_and_wait();
        y.store(1, StoreOrder);
        r2.store(x.load(LoadOrder), std::memory_order_relaxed);
        sync_point.arrive_and_wait();
      }
    });

    state.ResumeTiming();
    for (std::size_t round = 0; round < kLitmusRounds; ++round) {
      x.store(0, std::memory_order_relaxed);
      y.store(0, std::memory_order_relaxed);
      r1.store(-1, std::memory_order_relaxed);
      r2.store(-1, std::memory_order_relaxed);
      sync_point.arrive_and_wait();
      sync_point.arrive_and_wait();
      if (r1.load(std::memory_order_relaxed) == 0 &&
          r2.load(std::memory_order_relaxed) == 0) {
        ++both_zero;
      }
    }
    state.PauseTiming();

    t1.join();
    t2.join();
    state.counters[std::string(counter_prefix) + "_both_zero"] =
        static_cast<double>(both_zero);
    state.counters[std::string(counter_prefix) + "_both_zero_rate"] =
        static_cast<double>(both_zero) / static_cast<double>(kLitmusRounds);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kLitmusRounds));
}

void BM_StoreBufferingRelaxed(benchmark::State& state) {
  RunStoreBufferingLitmus<std::memory_order_relaxed, std::memory_order_relaxed>(
      state, "relaxed");
}

void BM_StoreBufferingReleaseAcquire(benchmark::State& state) {
  RunStoreBufferingLitmus<std::memory_order_release, std::memory_order_acquire>(
      state, "release_acquire");
}

void BM_StoreBufferingSeqCst(benchmark::State& state) {
  RunStoreBufferingLitmus<std::memory_order_seq_cst, std::memory_order_seq_cst>(
      state, "seq_cst");
}

}  // namespace

BENCHMARK(BM_FetchAddRelaxed)->Threads(1)->Threads(2)->Threads(4)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FetchAddAcqRel)->Threads(1)->Threads(2)->Threads(4)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FetchAddSeqCst)->Threads(1)->Threads(2)->Threads(4)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FlagHandoffAcqRel)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_FlagHandoffSeqCst)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PublishConsumeReleaseAcquire)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PublishConsumeSeqCst)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_RingTransferAcqRel)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_RingTransferSeqCst)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MessagePassingRelaxed)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MessagePassingReleaseAcquire)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StoreBufferingRelaxed)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StoreBufferingReleaseAcquire)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StoreBufferingSeqCst)->Unit(benchmark::kMillisecond);
