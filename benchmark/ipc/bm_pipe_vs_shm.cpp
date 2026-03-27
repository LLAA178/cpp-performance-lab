#include <benchmark/benchmark.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace {

constexpr std::size_t kMessageCount = 200'000;

struct alignas(64) Mailbox {
  std::atomic<int> full{0};
  std::uint64_t payload{0};
};

void BM_PipeThreadHandoff(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    int fds[2];
    if (::pipe(fds) != 0) {
      state.SkipWithError("pipe failed");
      return;
    }
    std::uint64_t checksum = 0;
    std::thread reader([&]() {
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < kMessageCount; ++i) {
        const ssize_t n = ::read(fds[0], &value, sizeof(value));
        if (n != static_cast<ssize_t>(sizeof(value))) {
          return;
        }
        checksum += value;
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t value = 0; value < kMessageCount; ++value) {
      const ssize_t n = ::write(fds[1], &value, sizeof(value));
      if (n != static_cast<ssize_t>(sizeof(value))) {
        state.SkipWithError("write failed");
        break;
      }
    }
    reader.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    ::close(fds[0]);
    ::close(fds[1]);
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kMessageCount / total_seconds;
}

void BM_ShmMailboxHandoff(benchmark::State& state) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    void* region = ::mmap(nullptr,
                          sizeof(Mailbox),
                          PROT_READ | PROT_WRITE,
                          MAP_ANON | MAP_SHARED,
                          -1,
                          0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }

    auto* mailbox = new (region) Mailbox();
    std::uint64_t checksum = 0;
    std::thread consumer([&]() {
      for (std::size_t i = 0; i < kMessageCount; ++i) {
        while (mailbox->full.load(std::memory_order_acquire) == 0) {
        }
        checksum += mailbox->payload;
        mailbox->full.store(0, std::memory_order_release);
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t value = 0; value < kMessageCount; ++value) {
      while (mailbox->full.load(std::memory_order_acquire) != 0) {
      }
      mailbox->payload = value;
      mailbox->full.store(1, std::memory_order_release);
    }
    consumer.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    benchmark::DoNotOptimize(checksum);
    mailbox->~Mailbox();
    ::munmap(region, sizeof(Mailbox));
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kMessageCount / total_seconds;
}

}  // namespace

BENCHMARK(BM_PipeThreadHandoff)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ShmMailboxHandoff)->Unit(benchmark::kMillisecond);
