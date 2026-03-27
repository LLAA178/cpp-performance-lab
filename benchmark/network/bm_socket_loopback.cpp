#include <benchmark/benchmark.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

namespace {

constexpr std::size_t kMessageCount = 100'000;
constexpr std::size_t kBatchMessages = 8;
constexpr std::size_t kPingPongCount = 50'000;

bool SendAll(int fd, const void* data, std::size_t bytes) {
  const auto* p = static_cast<const std::uint8_t*>(data);
  std::size_t sent = 0;
  while (sent < bytes) {
    const ssize_t n = ::send(fd, p + sent, bytes - sent, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool RecvAll(int fd, void* data, std::size_t bytes) {
  auto* p = static_cast<std::uint8_t*>(data);
  std::size_t recvd = 0;
  while (recvd < bytes) {
    const ssize_t n = ::recv(fd, p + recvd, bytes - recvd, 0);
    if (n <= 0) {
      return false;
    }
    recvd += static_cast<std::size_t>(n);
  }
  return true;
}

template <class SetupFn, class ConnectFn>
void RunLoopbackBenchmark(benchmark::State& state,
                          SetupFn setup_server,
                          ConnectFn connect_client,
                          std::size_t batch_messages) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    int listen_fd = -1;
    if (!setup_server(listen_fd)) {
      state.SkipWithError("server setup failed");
      return;
    }

    std::uint64_t checksum = 0;
    std::thread server([&]() {
      int conn = ::accept(listen_fd, nullptr, nullptr);
      if (conn < 0) {
        return;
      }
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < kMessageCount; i += batch_messages) {
        const std::size_t batch = std::min(batch_messages, kMessageCount - i);
        for (std::size_t j = 0; j < batch; ++j) {
          if (!RecvAll(conn, &value, sizeof(value))) {
            ::close(conn);
            return;
          }
          checksum += value;
        }
      }
      ::close(conn);
    });

    int client = -1;
    if (!connect_client(client)) {
      if (client >= 0) {
        ::close(client);
      }
      ::close(listen_fd);
      server.join();
      state.SkipWithError("client connect failed");
      return;
    }

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t value = 0; value < kMessageCount; value += batch_messages) {
      const std::size_t batch = std::min(batch_messages, kMessageCount - static_cast<std::size_t>(value));
      for (std::size_t j = 0; j < batch; ++j) {
        const std::uint64_t current = value + j;
        if (!SendAll(client, &current, sizeof(current))) {
          state.SkipWithError("send failed");
          break;
        }
      }
    }
    server.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    ::close(client);
    ::close(listen_fd);
    benchmark::DoNotOptimize(checksum);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["msgs_per_sec"] =
      static_cast<double>(state.iterations()) * kMessageCount / total_seconds;
  state.counters["batch_msgs"] = static_cast<double>(batch_messages);
}

void BM_TcpLoopback(benchmark::State& state) {
  sockaddr_in addr{};

  RunLoopbackBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        int opt = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        socklen_t len = sizeof(addr);
        return ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0;
      },
      [&](int& client) {
        client = ::socket(AF_INET, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      },
      1);
}

void BM_UnixStreamLoopback(benchmark::State& state) {
  sockaddr_un addr{};
  std::string path = "/tmp/cpp_perf_lab_unix_loopback_" + std::to_string(::getpid());

  RunLoopbackBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        addr = {};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
        ::unlink(addr.sun_path);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        return true;
      },
      [&](int& client) {
        client = ::socket(AF_UNIX, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      },
      1);

  ::unlink(path.c_str());
}

void BM_TcpLoopbackBatch8(benchmark::State& state) {
  sockaddr_in addr{};

  RunLoopbackBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        int opt = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        socklen_t len = sizeof(addr);
        return ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0;
      },
      [&](int& client) {
        client = ::socket(AF_INET, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      },
      kBatchMessages);
}

void BM_UnixStreamLoopbackBatch8(benchmark::State& state) {
  sockaddr_un addr{};
  std::string path = "/tmp/cpp_perf_lab_unix_loopback_batch_" + std::to_string(::getpid());

  RunLoopbackBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        addr = {};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
        ::unlink(addr.sun_path);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        return true;
      },
      [&](int& client) {
        client = ::socket(AF_UNIX, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      },
      kBatchMessages);

  ::unlink(path.c_str());
}

}  // namespace

BENCHMARK(BM_TcpLoopback)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_UnixStreamLoopback)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_TcpLoopbackBatch8)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_UnixStreamLoopbackBatch8)->Unit(benchmark::kMillisecond);

template <class SetupFn, class ConnectFn>
void RunPingPongBenchmark(benchmark::State& state, SetupFn setup_server, ConnectFn connect_client) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    int listen_fd = -1;
    if (!setup_server(listen_fd)) {
      state.SkipWithError("server setup failed");
      return;
    }

    std::thread server([&]() {
      int conn = ::accept(listen_fd, nullptr, nullptr);
      if (conn < 0) {
        return;
      }
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < kPingPongCount; ++i) {
        if (!RecvAll(conn, &value, sizeof(value))) {
          ::close(conn);
          return;
        }
        if (!SendAll(conn, &value, sizeof(value))) {
          ::close(conn);
          return;
        }
      }
      ::close(conn);
    });

    int client = -1;
    if (!connect_client(client)) {
      if (client >= 0) {
        ::close(client);
      }
      ::close(listen_fd);
      server.join();
      state.SkipWithError("client connect failed");
      return;
    }

    state.ResumeTiming();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint64_t value = 0; value < kPingPongCount; ++value) {
      if (!SendAll(client, &value, sizeof(value))) {
        state.SkipWithError("send failed");
        break;
      }
      std::uint64_t echo = 0;
      if (!RecvAll(client, &echo, sizeof(echo))) {
        state.SkipWithError("recv failed");
        break;
      }
      benchmark::DoNotOptimize(echo);
    }
    server.join();
    const auto t1 = std::chrono::steady_clock::now();
    state.PauseTiming();

    ::close(client);
    ::close(listen_fd);
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["round_trips_per_sec"] =
      static_cast<double>(state.iterations()) * kPingPongCount / total_seconds;
}

void BM_TcpPingPong(benchmark::State& state) {
  sockaddr_in addr{};
  RunPingPongBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        int opt = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        socklen_t len = sizeof(addr);
        return ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0;
      },
      [&](int& client) {
        client = ::socket(AF_INET, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      });
}

void BM_UnixStreamPingPong(benchmark::State& state) {
  sockaddr_un addr{};
  std::string path = "/tmp/cpp_perf_lab_unix_pingpong_" + std::to_string(::getpid());
  RunPingPongBenchmark(
      state,
      [&](int& listen_fd) {
        listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd < 0) {
          return false;
        }
        addr = {};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
        ::unlink(addr.sun_path);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
          return false;
        }
        return true;
      },
      [&](int& client) {
        client = ::socket(AF_UNIX, SOCK_STREAM, 0);
        return client >= 0 &&
               ::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
      });
  ::unlink(path.c_str());
}

BENCHMARK(BM_TcpPingPong)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_UnixStreamPingPong)->Unit(benchmark::kMillisecond);
