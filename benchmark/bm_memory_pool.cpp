#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kOpsPerThread = 200'000;
constexpr std::size_t kThreads = 4;

struct Node {
  alignas(64) std::array<std::uint8_t, 64> payload{};
  Node* next = nullptr;
};

class FixedPool {
 public:
  explicit FixedPool(std::size_t n) : storage_(n), free_head_(nullptr) {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      storage_[i].next = free_head_;
      free_head_ = &storage_[i];
    }
  }

  Node* allocate() {
    if (free_head_ == nullptr) {
      return nullptr;
    }
    Node* out = free_head_;
    free_head_ = free_head_->next;
    return out;
  }

  void deallocate(Node* p) {
    p->next = free_head_;
    free_head_ = p;
  }

 private:
  std::vector<Node> storage_;
  Node* free_head_;
};

class LockedPool {
 public:
  explicit LockedPool(std::size_t n) : pool_(n) {}

  Node* allocate() {
    std::lock_guard<std::mutex> lock(m_);
    return pool_.allocate();
  }

  void deallocate(Node* p) {
    std::lock_guard<std::mutex> lock(m_);
    pool_.deallocate(p);
  }

 private:
  FixedPool pool_;
  std::mutex m_;
};

void BM_NewDeleteMultiThread(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (std::size_t t = 0; t < kThreads; ++t) {
      threads.emplace_back([]() {
        std::vector<Node*> ptrs;
        ptrs.reserve(kOpsPerThread);
        for (std::size_t i = 0; i < kOpsPerThread; ++i) {
          ptrs.push_back(new Node());
        }
        for (Node* p : ptrs) {
          benchmark::DoNotOptimize(p->payload[0]);
          delete p;
        }
      });
    }
    for (auto& th : threads) {
      th.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  const double total_ops = static_cast<double>(state.iterations()) * kThreads * kOpsPerThread;
  state.counters["ops_per_sec"] = total_ops / total_seconds;
}

void BM_LockedPoolMultiThread(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    const auto t0 = std::chrono::steady_clock::now();
    LockedPool pool(kThreads * kOpsPerThread);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (std::size_t t = 0; t < kThreads; ++t) {
      threads.emplace_back([&pool]() {
        std::vector<Node*> ptrs;
        ptrs.reserve(kOpsPerThread);
        for (std::size_t i = 0; i < kOpsPerThread; ++i) {
          Node* p = nullptr;
          while ((p = pool.allocate()) == nullptr) {
            std::this_thread::yield();
          }
          ptrs.push_back(p);
        }
        for (Node* p : ptrs) {
          benchmark::DoNotOptimize(p->payload[0]);
          pool.deallocate(p);
        }
      });
    }
    for (auto& th : threads) {
      th.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  const double total_ops = static_cast<double>(state.iterations()) * kThreads * kOpsPerThread;
  state.counters["ops_per_sec"] = total_ops / total_seconds;
}

void BM_ThreadLocalPoolMultiThread(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (std::size_t t = 0; t < kThreads; ++t) {
      threads.emplace_back([]() {
        FixedPool pool(kOpsPerThread);
        std::vector<Node*> ptrs;
        ptrs.reserve(kOpsPerThread);
        for (std::size_t i = 0; i < kOpsPerThread; ++i) {
          Node* p = pool.allocate();
          ptrs.push_back(p);
        }
        for (Node* p : ptrs) {
          benchmark::DoNotOptimize(p->payload[0]);
          pool.deallocate(p);
        }
      });
    }
    for (auto& th : threads) {
      th.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  const double total_ops = static_cast<double>(state.iterations()) * kThreads * kOpsPerThread;
  state.counters["ops_per_sec"] = total_ops / total_seconds;
}

}  // namespace

BENCHMARK(BM_NewDeleteMultiThread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_LockedPoolMultiThread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ThreadLocalPoolMultiThread)->Unit(benchmark::kMillisecond);
