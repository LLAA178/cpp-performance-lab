#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <memory_resource>
#include <new>
#include <vector>

namespace {

constexpr std::size_t kObjectCount = 50'000;

struct Node {
  alignas(64) std::array<std::uint8_t, 64> payload{};
  Node* next = nullptr;
};

class ArenaPool {
 public:
  explicit ArenaPool(std::size_t count) : storage_(count), free_head_(nullptr) {
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

  void deallocate(Node* node) {
    node->next = free_head_;
    free_head_ = node;
  }

 private:
  std::vector<Node> storage_;
  Node* free_head_;
};

template <class AllocFn, class FreeFn>
void RunAllocatorBenchmark(benchmark::State& state, AllocFn alloc_fn, FreeFn free_fn) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Node*> ptrs;
    ptrs.reserve(kObjectCount);
    for (std::size_t i = 0; i < kObjectCount; ++i) {
      ptrs.push_back(alloc_fn());
    }
    for (Node* node : ptrs) {
      benchmark::DoNotOptimize(node->payload[0]);
      free_fn(node);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * kObjectCount / total_seconds;
}

void BM_NewDelete(benchmark::State& state) {
  RunAllocatorBenchmark(
      state,
      []() { return new Node(); },
      [](Node* node) { delete node; });
}

void BM_MallocFree(benchmark::State& state) {
  RunAllocatorBenchmark(
      state,
      []() {
        void* p = std::malloc(sizeof(Node));
        return new (p) Node();
      },
      [](Node* node) {
        node->~Node();
        std::free(node);
      });
}

void BM_PmrMonotonic(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    std::vector<std::byte> buffer(kObjectCount * sizeof(Node) + 4096);
    std::pmr::monotonic_buffer_resource resource(buffer.data(), buffer.size());
    std::pmr::polymorphic_allocator<Node> alloc(&resource);

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Node*> ptrs;
    ptrs.reserve(kObjectCount);
    for (std::size_t i = 0; i < kObjectCount; ++i) {
      ptrs.push_back(alloc.allocate(1));
      std::construct_at(ptrs.back());
    }
    for (Node* node : ptrs) {
      benchmark::DoNotOptimize(node->payload[0]);
      std::destroy_at(node);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * kObjectCount / total_seconds;
}

void BM_PmrUnsyncPool(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    std::pmr::unsynchronized_pool_resource resource;
    std::pmr::polymorphic_allocator<Node> alloc(&resource);

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Node*> ptrs;
    ptrs.reserve(kObjectCount);
    for (std::size_t i = 0; i < kObjectCount; ++i) {
      ptrs.push_back(alloc.allocate(1));
      std::construct_at(ptrs.back());
    }
    for (Node* node : ptrs) {
      benchmark::DoNotOptimize(node->payload[0]);
      std::destroy_at(node);
      alloc.deallocate(node, 1);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * kObjectCount / total_seconds;
}

void BM_ArenaPool(benchmark::State& state) {
  double total_seconds = 0.0;
  for (auto _ : state) {
    ArenaPool pool(kObjectCount);
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Node*> ptrs;
    ptrs.reserve(kObjectCount);
    for (std::size_t i = 0; i < kObjectCount; ++i) {
      ptrs.push_back(pool.allocate());
    }
    for (Node* node : ptrs) {
      benchmark::DoNotOptimize(node->payload[0]);
      pool.deallocate(node);
    }
    const auto t1 = std::chrono::steady_clock::now();
    total_seconds += std::chrono::duration<double>(t1 - t0).count();
  }

  state.counters["ops_per_sec"] =
      static_cast<double>(state.iterations()) * kObjectCount / total_seconds;
}

}  // namespace

BENCHMARK(BM_NewDelete)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MallocFree)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PmrMonotonic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PmrUnsyncPool)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ArenaPool)->Unit(benchmark::kMillisecond);
