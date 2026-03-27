#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory_resource>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kObjectCount = 200'000;

struct Node {
  alignas(64) std::uint8_t payload[64]{};
  Node* next = nullptr;
};

class LockedPool {
 public:
  explicit LockedPool(std::size_t count) : storage_(count) {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      storage_[i].next = free_head_;
      free_head_ = &storage_[i];
    }
  }

  Node* allocate() {
    std::lock_guard<std::mutex> guard(m_);
    if (free_head_ == nullptr) {
      return nullptr;
    }
    Node* out = free_head_;
    free_head_ = free_head_->next;
    return out;
  }

  void deallocate(Node* p) {
    std::lock_guard<std::mutex> guard(m_);
    p->next = free_head_;
    free_head_ = p;
  }

 private:
  std::vector<Node> storage_;
  Node* free_head_ = nullptr;
  std::mutex m_;
};

template <class AllocFn, class FreeFn>
void RunCrossThreadBenchmark(benchmark::State& state, AllocFn alloc_fn, FreeFn free_fn) {
  double total_seconds = 0.0;

  for (auto _ : state) {
    state.PauseTiming();
    std::vector<Node*> slots(kObjectCount, nullptr);
    std::atomic<bool> start{false};
    std::atomic<std::size_t> produced{0};
    std::uint64_t checksum = 0;

    std::thread producer([&]() {
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kObjectCount; ++i) {
        Node* node = nullptr;
        while ((node = alloc_fn()) == nullptr) {
        }
        node->payload[0] = static_cast<std::uint8_t>(i);
        slots[i] = node;
        produced.store(i + 1, std::memory_order_release);
      }
    });

    std::thread consumer([&]() {
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < kObjectCount; ++i) {
        while (produced.load(std::memory_order_acquire) <= i) {
        }
        Node* node = slots[i];
        checksum += node->payload[0];
        free_fn(node);
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
      static_cast<double>(state.iterations()) * kObjectCount / total_seconds;
}

void BM_NewDeleteCrossThread(benchmark::State& state) {
  RunCrossThreadBenchmark(
      state,
      []() { return new Node(); },
      [](Node* node) { delete node; });
}

void BM_MallocFreeCrossThread(benchmark::State& state) {
  RunCrossThreadBenchmark(
      state,
      []() {
        void* p = std::malloc(sizeof(Node));
        return p == nullptr ? nullptr : new (p) Node();
      },
      [](Node* node) {
        node->~Node();
        std::free(node);
      });
}

void BM_LockedPoolCrossThread(benchmark::State& state) {
  LockedPool pool(kObjectCount);
  RunCrossThreadBenchmark(
      state,
      [&]() { return pool.allocate(); },
      [&](Node* node) { pool.deallocate(node); });
}

void BM_PmrSyncPoolCrossThread(benchmark::State& state) {
  std::pmr::synchronized_pool_resource resource;
  RunCrossThreadBenchmark(
      state,
      [&]() {
        void* p = resource.allocate(sizeof(Node), alignof(Node));
        return p == nullptr ? nullptr : new (p) Node();
      },
      [&](Node* node) {
        node->~Node();
        resource.deallocate(node, sizeof(Node), alignof(Node));
      });
}

}  // namespace

BENCHMARK(BM_NewDeleteCrossThread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MallocFreeCrossThread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_LockedPoolCrossThread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PmrSyncPoolCrossThread)->Unit(benchmark::kMillisecond);
