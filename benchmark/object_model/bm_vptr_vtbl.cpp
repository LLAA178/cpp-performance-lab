#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace {

constexpr std::size_t kElementCount = 1 << 20;
constexpr std::size_t kOpsPerIteration = 8;
constexpr std::size_t kLifecycleSamples = 4;
constexpr std::size_t kVtableProbeRepeats = 1 << 12;

#if defined(__clang__) || defined(__GNUC__)
#define BM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define BM_NOINLINE __declspec(noinline)
#else
#define BM_NOINLINE
#endif

// This probes the first machine word of an object as a proxy for the active
// vptr. That matches mainstream C++ ABIs in practice, but it is not a source-
// level language guarantee.
std::uintptr_t ReadFirstWord(const void* object) {
  std::uintptr_t word = 0;
  std::memcpy(&word, object, sizeof(word));
  return word;
}

struct PlainLeaf {
  std::uint64_t payload;
  std::uint64_t bias;

  std::uint64_t Mix() const { return (payload * 3u) + (bias * 7u); }
};

struct PolyBase {
  std::uint64_t payload;

  explicit PolyBase(std::uint64_t v) : payload(v) {}
  virtual ~PolyBase() = default;
  virtual std::uint64_t Mix() const = 0;
};

struct PolyLeafA final : PolyBase {
  std::uint64_t bias;

  PolyLeafA(std::uint64_t payload_value, std::uint64_t bias_value)
      : PolyBase(payload_value), bias(bias_value) {}

  std::uint64_t Mix() const override { return (payload * 3u) + (bias * 7u); }
};

struct PolyLeafB final : PolyBase {
  std::uint64_t bias;

  PolyLeafB(std::uint64_t payload_value, std::uint64_t bias_value)
      : PolyBase(payload_value), bias(bias_value) {}

  std::uint64_t Mix() const override { return (payload * 5u) + (bias * 11u); }
};

struct LifecycleProbeBase {
  static inline std::array<std::uintptr_t, kLifecycleSamples> samples{};
  static inline std::size_t sample_count = 0;
  static inline std::array<std::uint64_t, kLifecycleSamples> virtual_results{};

  std::uint64_t payload = 1;

  static void Reset() {
    samples.fill(0);
    virtual_results.fill(0);
    sample_count = 0;
  }

  static void Record(const LifecycleProbeBase* self, std::uint64_t result) {
    if (sample_count < samples.size()) {
      samples[sample_count] = ReadFirstWord(self);
      virtual_results[sample_count] = result;
      ++sample_count;
    }
  }

  BM_NOINLINE LifecycleProbeBase() { Record(this, VirtualTag()); }

  BM_NOINLINE virtual ~LifecycleProbeBase() { Record(this, VirtualTag()); }

  BM_NOINLINE virtual std::uint64_t VirtualTag() const { return 0xBACEu; }
};

struct LifecycleProbeDerived final : LifecycleProbeBase {
  std::uint64_t bias = 7;

  BM_NOINLINE LifecycleProbeDerived() { Record(this, VirtualTag()); }

  BM_NOINLINE ~LifecycleProbeDerived() override { Record(this, VirtualTag()); }

  BM_NOINLINE std::uint64_t VirtualTag() const override { return 0xD00Du; }
};

struct PlainCtorObject {
  std::uint64_t a;
  std::uint64_t b;
  std::uint64_t c;
  std::uint64_t d;

  explicit PlainCtorObject(std::uint64_t seed)
      : a(seed),
        b(seed * 3u),
        c(seed ^ 0x9e3779b97f4a7c15ull),
        d((seed << 1u) | 1u) {}

  std::uint64_t Touch() const { return a ^ b ^ c ^ d; }
};

struct VirtualCtorObject {
  std::uint64_t a;
  std::uint64_t b;
  std::uint64_t c;
  std::uint64_t d;

  explicit VirtualCtorObject(std::uint64_t seed)
      : a(seed),
        b(seed * 3u),
        c(seed ^ 0x9e3779b97f4a7c15ull),
        d((seed << 1u) | 1u) {}

  virtual ~VirtualCtorObject() = default;
  virtual std::uint64_t Touch() const { return a ^ b ^ c ^ d; }
};

struct VirtualCtorLeaf final : VirtualCtorObject {
  explicit VirtualCtorLeaf(std::uint64_t seed) : VirtualCtorObject(seed) {}

  std::uint64_t Touch() const override { return a ^ b ^ c ^ d; }
};

struct SingleVtableBase {
  virtual std::uint64_t Ping() const { return 1; }
  virtual std::uint64_t Pong() const { return 2; }
};

struct SingleVtableDerived final : SingleVtableBase {
  std::uint64_t Ping() const override { return 11; }
  std::uint64_t Pong() const override { return 22; }
};

struct LeftVtableBase {
  virtual std::uint64_t LeftPing() const { return 3; }
  virtual std::uint64_t LeftPong() const { return 4; }
};

struct RightVtableBase {
  virtual std::uint64_t RightPing() const { return 5; }
  virtual std::uint64_t RightPong() const { return 6; }
};

struct MultiVtableDerived final : LeftVtableBase, RightVtableBase {
  std::uint64_t LeftPing() const override { return 31; }
  std::uint64_t LeftPong() const override { return 32; }
  std::uint64_t RightPing() const override { return 41; }
  std::uint64_t RightPong() const override { return 42; }
};

struct LayoutStats {
  std::size_t plain_size = 0;
  std::size_t polymorphic_size = 0;
  std::size_t extra_bytes = 0;
  std::size_t plain_payload_offset = 0;
  std::size_t polymorphic_payload_offset = 0;
  std::size_t align_plain = 0;
  std::size_t align_polymorphic = 0;
};

LayoutStats GetLayoutStats() {
  PlainLeaf plain{11, 17};
  PolyLeafA polymorphic{11, 17};

  const auto* plain_bytes = reinterpret_cast<const char*>(&plain);
  const auto* poly_bytes = reinterpret_cast<const char*>(&polymorphic);

  LayoutStats stats;
  stats.plain_size = sizeof(PlainLeaf);
  stats.polymorphic_size = sizeof(PolyLeafA);
  stats.extra_bytes = sizeof(PolyLeafA) - sizeof(PlainLeaf);
  stats.plain_payload_offset =
      static_cast<std::size_t>(reinterpret_cast<const char*>(&plain.payload) - plain_bytes);
  stats.polymorphic_payload_offset =
      static_cast<std::size_t>(reinterpret_cast<const char*>(&polymorphic.payload) - poly_bytes);
  stats.align_plain = alignof(PlainLeaf);
  stats.align_polymorphic = alignof(PolyLeafA);
  return stats;
}

struct VtableSnapshot {
  std::uintptr_t address_point = 0;
  std::intptr_t offset_to_top = 0;
  std::uintptr_t typeinfo_ptr = 0;
  std::array<std::uintptr_t, 2> slots{};
};

template <typename Expected, typename Base>
VtableSnapshot InspectVtable(const Base* object) {
  VtableSnapshot snapshot;
  snapshot.address_point = ReadFirstWord(object);

  const auto* table = reinterpret_cast<const std::uintptr_t*>(snapshot.address_point);
  std::memcpy(&snapshot.offset_to_top, table - 2, sizeof(snapshot.offset_to_top));
  std::memcpy(&snapshot.typeinfo_ptr, table - 1, sizeof(snapshot.typeinfo_ptr));
  std::memcpy(&snapshot.slots[0], table + 0, sizeof(snapshot.slots[0]));
  std::memcpy(&snapshot.slots[1], table + 1, sizeof(snapshot.slots[1]));
  (void)Expected{};
  return snapshot;
}

template <typename Base>
using VtableSlotFn = std::uint64_t (*)(const Base*);

template <typename Base>
std::uint64_t InvokeVtableSlot(std::uintptr_t slot, const Base* object) {
  return reinterpret_cast<VtableSlotFn<Base>>(slot)(object);
}

const std::vector<PlainLeaf>& GetPlainObjects() {
  static const std::vector<PlainLeaf> objects = [] {
    std::vector<PlainLeaf> values(kElementCount);
    for (std::size_t i = 0; i < values.size(); ++i) {
      values[i] = PlainLeaf{static_cast<std::uint64_t>(i + 1),
                            static_cast<std::uint64_t>((i * 17u) ^ 0x9e3779b9u)};
    }
    return values;
  }();
  return objects;
}

const std::vector<PolyLeafA>& GetMonomorphicObjects() {
  static const std::vector<PolyLeafA> objects = [] {
    std::vector<PolyLeafA> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < kElementCount; ++i) {
      values.emplace_back(static_cast<std::uint64_t>(i + 1),
                          static_cast<std::uint64_t>((i * 17u) ^ 0x9e3779b9u));
    }
    return values;
  }();
  return objects;
}

const std::vector<const PolyBase*>& GetMonomorphicPointers() {
  static const std::vector<const PolyBase*> pointers = [] {
    const auto& objects = GetMonomorphicObjects();
    std::vector<const PolyBase*> values;
    values.reserve(objects.size());
    for (const auto& object : objects) {
      values.push_back(&object);
    }
    return values;
  }();
  return pointers;
}

const std::vector<std::unique_ptr<PolyBase>>& GetPolymorphicOwners() {
  static const std::vector<std::unique_ptr<PolyBase>> owners = [] {
    std::vector<std::unique_ptr<PolyBase>> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < kElementCount; ++i) {
      const auto payload = static_cast<std::uint64_t>(i + 1);
      const auto bias = static_cast<std::uint64_t>((i * 17u) ^ 0x9e3779b9u);
      if ((i & 1u) == 0u) {
        values.push_back(std::make_unique<PolyLeafA>(payload, bias));
      } else {
        values.push_back(std::make_unique<PolyLeafB>(payload, bias));
      }
    }
    return values;
  }();
  return owners;
}

const std::vector<const PolyBase*>& GetPolymorphicPointers() {
  static const std::vector<const PolyBase*> pointers = [] {
    const auto& owners = GetPolymorphicOwners();
    std::vector<const PolyBase*> values;
    values.reserve(owners.size());
    for (const auto& owner : owners) {
      values.push_back(owner.get());
    }
    return values;
  }();
  return pointers;
}

void BM_VptrLayoutFootprint(benchmark::State& state) {
  const LayoutStats stats = GetLayoutStats();
  PlainLeaf plain{11, 17};
  PolyLeafA polymorphic{11, 17};
  std::uintptr_t checksum = 0;

  for (auto _ : state) {
    checksum ^= ReadFirstWord(&plain);
    checksum ^= ReadFirstWord(&polymorphic);
    benchmark::DoNotOptimize(checksum);
  }

  state.counters["plain_bytes"] = static_cast<double>(stats.plain_size);
  state.counters["poly_bytes"] = static_cast<double>(stats.polymorphic_size);
  state.counters["extra_bytes"] = static_cast<double>(stats.extra_bytes);
  state.counters["plain_payload_off"] = static_cast<double>(stats.plain_payload_offset);
  state.counters["poly_payload_off"] = static_cast<double>(stats.polymorphic_payload_offset);
  state.counters["ptr_bytes"] = static_cast<double>(sizeof(void*));
  state.counters["plain_align"] = static_cast<double>(stats.align_plain);
  state.counters["poly_align"] = static_cast<double>(stats.align_polymorphic);
  state.SetLabel("first word behaves like active vptr on mainstream ABIs");
}

void BM_VptrLifecycleTransitions(benchmark::State& state) {
  std::uintptr_t checksum = 0;
  std::size_t unique_count = 0;
  std::size_t transition_count = 0;
  std::array<std::uint64_t, kLifecycleSamples> tags{};

  for (auto _ : state) {
    LifecycleProbeBase::Reset();
    {
      LifecycleProbeDerived object;
      benchmark::DoNotOptimize(object.payload);
      benchmark::ClobberMemory();
    }

    checksum ^= LifecycleProbeBase::samples[0];
    std::array<std::uintptr_t, kLifecycleSamples> sorted = LifecycleProbeBase::samples;
    std::sort(sorted.begin(), sorted.end());
    unique_count =
        static_cast<std::size_t>(std::unique(sorted.begin(), sorted.end()) - sorted.begin());
    transition_count = 0;
    for (std::size_t i = 1; i < LifecycleProbeBase::sample_count; ++i) {
      if (LifecycleProbeBase::samples[i] != LifecycleProbeBase::samples[i - 1]) {
        ++transition_count;
      }
    }
    tags = LifecycleProbeBase::virtual_results;
    benchmark::DoNotOptimize(checksum);
  }

  state.counters["samples"] = static_cast<double>(LifecycleProbeBase::sample_count);
  state.counters["unique_vptrs"] = static_cast<double>(unique_count);
  state.counters["vptr_switches"] = static_cast<double>(transition_count);
  state.counters["base_ctor_tag"] = static_cast<double>(tags[0]);
  state.counters["derived_ctor_tag"] = static_cast<double>(tags[1]);
  state.counters["derived_dtor_tag"] = static_cast<double>(tags[2]);
  state.counters["base_dtor_tag"] = static_cast<double>(tags[3]);
  state.SetLabel("expected order: base ctor -> derived ctor -> derived dtor -> base dtor");
}

void BM_VtableLayoutSingleInheritance(benchmark::State& state) {
  SingleVtableDerived object;
  const SingleVtableBase* base = &object;
  VtableSnapshot snapshot{};
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kVtableProbeRepeats; ++i) {
      snapshot = InspectVtable<SingleVtableDerived>(base);
      checksum ^= snapshot.address_point;
      checksum ^= static_cast<std::uint64_t>(snapshot.offset_to_top);
      checksum ^= snapshot.typeinfo_ptr;
      checksum ^= InvokeVtableSlot<SingleVtableBase>(snapshot.slots[0], base);
      checksum ^= InvokeVtableSlot<SingleVtableBase>(snapshot.slots[1], base);
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.counters["offset_to_top"] = static_cast<double>(snapshot.offset_to_top);
  state.counters["typeinfo_match"] =
      snapshot.typeinfo_ptr == reinterpret_cast<std::uintptr_t>(&typeid(SingleVtableDerived))
          ? 1.0
          : 0.0;
  state.counters["slot0_result"] =
      static_cast<double>(InvokeVtableSlot<SingleVtableBase>(snapshot.slots[0], base));
  state.counters["slot1_result"] =
      static_cast<double>(InvokeVtableSlot<SingleVtableBase>(snapshot.slots[1], base));
  state.SetLabel("single inheritance: [offset-to-top][typeinfo][virtual slots]");
}

void BM_VtableLayoutMultipleInheritance(benchmark::State& state) {
  MultiVtableDerived object;
  const LeftVtableBase* left = &object;
  const RightVtableBase* right = &object;
  VtableSnapshot left_snapshot{};
  VtableSnapshot right_snapshot{};
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kVtableProbeRepeats; ++i) {
      left_snapshot = InspectVtable<MultiVtableDerived>(left);
      right_snapshot = InspectVtable<MultiVtableDerived>(right);
      checksum ^= left_snapshot.address_point ^ right_snapshot.address_point;
      checksum ^= static_cast<std::uint64_t>(left_snapshot.offset_to_top);
      checksum ^= static_cast<std::uint64_t>(right_snapshot.offset_to_top);
      checksum ^= left_snapshot.typeinfo_ptr ^ right_snapshot.typeinfo_ptr;
      checksum ^= InvokeVtableSlot<LeftVtableBase>(left_snapshot.slots[0], left);
      checksum ^= InvokeVtableSlot<LeftVtableBase>(left_snapshot.slots[1], left);
      checksum ^= InvokeVtableSlot<RightVtableBase>(right_snapshot.slots[0], right);
      checksum ^= InvokeVtableSlot<RightVtableBase>(right_snapshot.slots[1], right);
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.counters["left_offset"] = static_cast<double>(left_snapshot.offset_to_top);
  state.counters["right_offset"] = static_cast<double>(right_snapshot.offset_to_top);
  state.counters["shared_typeinfo"] =
      left_snapshot.typeinfo_ptr == right_snapshot.typeinfo_ptr ? 1.0 : 0.0;
  state.counters["left_slot0_result"] =
      static_cast<double>(InvokeVtableSlot<LeftVtableBase>(left_snapshot.slots[0], left));
  state.counters["left_slot1_result"] =
      static_cast<double>(InvokeVtableSlot<LeftVtableBase>(left_snapshot.slots[1], left));
  state.counters["right_slot0_result"] =
      static_cast<double>(InvokeVtableSlot<RightVtableBase>(right_snapshot.slots[0], right));
  state.counters["right_slot1_result"] =
      static_cast<double>(InvokeVtableSlot<RightVtableBase>(right_snapshot.slots[1], right));
  state.SetLabel("multiple inheritance: primary and secondary vptrs share typeinfo but differ in offset-to-top");
}

void BM_ConstructDestroyPlain(benchmark::State& state) {
  std::uint64_t checksum = 0;
  std::uint64_t seed = 1;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerIteration * 1024; ++i) {
      PlainCtorObject object(seed++);
      checksum ^= object.Touch();
      benchmark::DoNotOptimize(object);
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(kOpsPerIteration * 1024));
}

void BM_ConstructDestroyVirtual(benchmark::State& state) {
  std::uint64_t checksum = 0;
  std::uint64_t seed = 1;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kOpsPerIteration * 1024; ++i) {
      VirtualCtorLeaf object(seed++);
      checksum ^= object.Touch();
      benchmark::DoNotOptimize(object);
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(kOpsPerIteration * 1024));
}

void BM_ScanPlainObjects(benchmark::State& state) {
  const auto& objects = GetPlainObjects();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const PlainLeaf& object : objects) {
        checksum += object.payload;
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * sizeof(PlainLeaf) *
                                               kOpsPerIteration));
}

void BM_ScanPolymorphicObjects(benchmark::State& state) {
  const auto& objects = GetMonomorphicObjects();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const PolyLeafA& object : objects) {
        checksum += object.payload;
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * sizeof(PolyLeafA) *
                                               kOpsPerIteration));
}

void BM_DirectCall(benchmark::State& state) {
  const auto& objects = GetPlainObjects();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const PlainLeaf& object : objects) {
        checksum += object.Mix();
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
}

void BM_VirtualCallMonomorphic(benchmark::State& state) {
  const auto& objects = GetMonomorphicPointers();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const PolyBase* object : objects) {
        checksum += object->Mix();
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
}

void BM_VirtualCallPolymorphic(benchmark::State& state) {
  const auto& objects = GetPolymorphicPointers();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const PolyBase* object : objects) {
        checksum += object->Mix();
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
}

#undef BM_NOINLINE

}  // namespace

BENCHMARK(BM_VptrLayoutFootprint)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_VptrLifecycleTransitions)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_VtableLayoutSingleInheritance)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_VtableLayoutMultipleInheritance)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_ConstructDestroyPlain)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_ConstructDestroyVirtual)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_ScanPlainObjects)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_ScanPolymorphicObjects)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_DirectCall)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_VirtualCallMonomorphic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_VirtualCallPolymorphic)->Unit(benchmark::kMillisecond);
