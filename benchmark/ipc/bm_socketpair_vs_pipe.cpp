#include <benchmark/benchmark.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace {

constexpr std::size_t kMessageCount = 200'000;

template <typename CreateFn, typename ReadFn, typename WriteFn>
void RunFdPairBenchmark(benchmark::State& state,
                        CreateFn create_pair,
                        ReadFn read_fn,
                        WriteFn write_fn) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    int fds[2];
    if (!create_pair(fds)) {
      state.SkipWithError("fd pair creation failed");
      return;
    }

    std::uint64_t checksum = 0;
    std::thread reader([&]() {
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < kMessageCount; ++i) {
        const ssize_t n = read_fn(fds[0], &value, sizeof(value));
        if (n != static_cast<ssize_t>(sizeof(value))) {
          return;
        }
        checksum += value;
      }
    });

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t value = 0; value < kMessageCount; ++value) {
      const ssize_t n = write_fn(fds[1], &value, sizeof(value));
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

void BM_PipePair(benchmark::State& state) {
  RunFdPairBenchmark(
      state,
      [](int fds[2]) { return ::pipe(fds) == 0; },
      [](int fd, void* buf, std::size_t len) { return ::read(fd, buf, len); },
      [](int fd, const void* buf, std::size_t len) { return ::write(fd, buf, len); });
}

void BM_SocketPairStream(benchmark::State& state) {
  RunFdPairBenchmark(
      state,
      [](int fds[2]) { return ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0; },
      [](int fd, void* buf, std::size_t len) { return ::recv(fd, buf, len, 0); },
      [](int fd, const void* buf, std::size_t len) { return ::send(fd, buf, len, 0); });
}

}  // namespace

BENCHMARK(BM_PipePair)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SocketPairStream)->Unit(benchmark::kMillisecond);
