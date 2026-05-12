#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <typeinfo>
#include <vector>

namespace {

constexpr std::size_t kElementCount = 1 << 18;
constexpr std::size_t kOpsPerIteration = 16;
constexpr std::size_t kProbeRepeats = 1 << 12;

#if defined(__clang__) || defined(__GNUC__)
#define BM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define BM_NOINLINE __declspec(noinline)
#else
#define BM_NOINLINE
#endif

std::uintptr_t ReadFirstWord(const void* object) {
  std::uintptr_t word = 0;
  std::memcpy(&word, object, sizeof(word));
  return word;
}

std::intptr_t ByteOffset(const void* from, const void* to) {
  return reinterpret_cast<const char*>(to) - reinterpret_cast<const char*>(from);
}

struct DirectObject {
  std::uint64_t shared;
  std::uint64_t left_bias;
  std::uint64_t right_bias;
  std::uint64_t payload;
};

struct NonVirtualSharedBase {
  std::uint64_t shared;

  explicit NonVirtualSharedBase(std::uint64_t seed) : shared(seed) {}
};

struct NonVirtualLeft : NonVirtualSharedBase {
  std::uint64_t left_bias;

  explicit NonVirtualLeft(std::uint64_t seed)
      : NonVirtualSharedBase(seed), left_bias(seed * 3u + 1u) {}
};

struct NonVirtualRight : NonVirtualSharedBase {
  std::uint64_t right_bias;

  explicit NonVirtualRight(std::uint64_t seed)
      : NonVirtualSharedBase(seed), right_bias(seed * 5u + 7u) {}
};

struct NonVirtualDiamondA : NonVirtualLeft, NonVirtualRight {
  std::uint64_t payload;

  explicit NonVirtualDiamondA(std::uint64_t seed)
      : NonVirtualLeft(seed),
        NonVirtualRight(seed + 1u),
        payload(seed ^ 0x9e3779b97f4a7c15ull) {}
};

struct VirtualSharedBase {
  std::uint64_t shared = 0;

  explicit VirtualSharedBase(std::uint64_t seed = 0) : shared(seed) {}
};

struct VirtualLeft : virtual VirtualSharedBase {
  std::uint64_t left_bias = 0;

  explicit VirtualLeft(std::uint64_t seed = 0) : left_bias(seed * 3u + 1u) {}
  virtual std::uint64_t AnchorLeft() const { return 0x1111u; }
};

struct VirtualRight : virtual VirtualSharedBase {
  std::uint64_t right_bias = 0;

  explicit VirtualRight(std::uint64_t seed = 0) : right_bias(seed * 5u + 7u) {}
  virtual std::uint64_t AnchorRight() const { return 0x2222u; }
};

struct VirtualDiamondA final : VirtualLeft, VirtualRight {
  std::uint64_t payload;

  explicit VirtualDiamondA(std::uint64_t seed)
      : VirtualSharedBase(seed),
        VirtualLeft(seed),
        VirtualRight(seed),
        payload(seed ^ 0x9e3779b97f4a7c15ull) {}

  std::uint64_t AnchorLeft() const override { return 0xAAA1u; }
  std::uint64_t AnchorRight() const override { return 0xAAA2u; }
};

struct VirtualDiamondB final : VirtualLeft, VirtualRight {
  std::uint64_t payload;
  std::array<std::uint64_t, 3> extra;

  explicit VirtualDiamondB(std::uint64_t seed)
      : VirtualSharedBase(seed),
        VirtualLeft(seed),
        VirtualRight(seed),
        payload((seed << 1u) | 1u),
        extra{seed + 11u, seed + 13u, seed + 17u} {}

  std::uint64_t AnchorLeft() const override { return 0xBBB1u; }
  std::uint64_t AnchorRight() const override { return 0xBBB2u; }
};

struct VtableSnapshot {
  std::uintptr_t address_point = 0;
  std::intptr_t offset_to_top = 0;
  std::uintptr_t typeinfo_ptr = 0;
  std::uintptr_t slot0 = 0;
};

template <typename Base>
VtableSnapshot InspectVtable(const Base* object) {
  VtableSnapshot snapshot;
  snapshot.address_point = ReadFirstWord(object);

  const auto* table = reinterpret_cast<const std::uintptr_t*>(snapshot.address_point);
  std::memcpy(&snapshot.offset_to_top, table - 2, sizeof(snapshot.offset_to_top));
  std::memcpy(&snapshot.typeinfo_ptr, table - 1, sizeof(snapshot.typeinfo_ptr));
  std::memcpy(&snapshot.slot0, table + 0, sizeof(snapshot.slot0));
  return snapshot;
}

template <typename Base>
using VtableSlotFn = std::uint64_t (*)(const Base*);

template <typename Base>
std::uint64_t InvokeVtableSlot(std::uintptr_t slot, const Base* object) {
  return reinterpret_cast<VtableSlotFn<Base>>(slot)(object);
}

struct VbaseEntryMatch {
  std::size_t found = 0;
  int index = 0;
};

VbaseEntryMatch FindVbaseDeltaEntry(std::uintptr_t address_point, std::intptr_t target_delta) {
  const auto* table = reinterpret_cast<const std::intptr_t*>(address_point);
  for (int index = -8; index < 0; ++index) {
    std::intptr_t value = 0;
    std::memcpy(&value, table + index, sizeof(value));
    if (value == target_delta) {
      return {1u, index};
    }
  }
  return {};
}

struct VirtualBaseAbiProbe {
  std::size_t direct_size = 0;
  std::size_t non_virtual_size = 0;
  std::size_t virtual_a_size = 0;
  std::size_t virtual_b_size = 0;
  std::size_t left_size = 0;
  std::size_t right_size = 0;
  std::size_t shared_size = 0;
  std::intptr_t left_offset = 0;
  std::intptr_t right_offset = 0;
  std::intptr_t shared_from_top = 0;
  std::intptr_t shared_from_left = 0;
  std::intptr_t shared_from_right = 0;
  std::size_t shared_alias = 0;
  std::intptr_t left_offset_to_top = 0;
  std::intptr_t right_offset_to_top = 0;
  std::size_t shared_typeinfo = 0;
  std::uint64_t left_slot_result = 0;
  std::uint64_t right_slot_result = 0;
  std::size_t left_delta_found = 0;
  std::size_t right_delta_found = 0;
  int left_delta_index = 0;
  int right_delta_index = 0;
};

VirtualBaseAbiProbe GetAbiProbe() {
  VirtualDiamondA object{11};

  const auto* top = static_cast<const void*>(&object);
  const VirtualLeft* left = &object;
  const VirtualRight* right = &object;
  const VirtualSharedBase* shared_from_left = left;
  const VirtualSharedBase* shared_from_right = right;

  const VtableSnapshot left_snapshot = InspectVtable(left);
  const VtableSnapshot right_snapshot = InspectVtable(right);

  const VbaseEntryMatch left_match =
      FindVbaseDeltaEntry(left_snapshot.address_point,
                          ByteOffset(static_cast<const void*>(left),
                                     static_cast<const void*>(shared_from_left)));
  const VbaseEntryMatch right_match =
      FindVbaseDeltaEntry(right_snapshot.address_point,
                          ByteOffset(static_cast<const void*>(right),
                                     static_cast<const void*>(shared_from_right)));

  VirtualBaseAbiProbe probe;
  probe.direct_size = sizeof(DirectObject);
  probe.non_virtual_size = sizeof(NonVirtualDiamondA);
  probe.virtual_a_size = sizeof(VirtualDiamondA);
  probe.virtual_b_size = sizeof(VirtualDiamondB);
  probe.left_size = sizeof(VirtualLeft);
  probe.right_size = sizeof(VirtualRight);
  probe.shared_size = sizeof(VirtualSharedBase);
  probe.left_offset = ByteOffset(top, left);
  probe.right_offset = ByteOffset(top, right);
  probe.shared_from_top = ByteOffset(top, shared_from_left);
  probe.shared_from_left =
      ByteOffset(static_cast<const void*>(left), static_cast<const void*>(shared_from_left));
  probe.shared_from_right =
      ByteOffset(static_cast<const void*>(right), static_cast<const void*>(shared_from_right));
  probe.shared_alias = shared_from_left == shared_from_right ? 1u : 0u;
  probe.left_offset_to_top = left_snapshot.offset_to_top;
  probe.right_offset_to_top = right_snapshot.offset_to_top;
  probe.shared_typeinfo =
      left_snapshot.typeinfo_ptr == right_snapshot.typeinfo_ptr &&
              left_snapshot.typeinfo_ptr ==
                  reinterpret_cast<std::uintptr_t>(&typeid(VirtualDiamondA))
          ? 1u
          : 0u;
  probe.left_slot_result = InvokeVtableSlot<VirtualLeft>(left_snapshot.slot0, left);
  probe.right_slot_result = InvokeVtableSlot<VirtualRight>(right_snapshot.slot0, right);
  probe.left_delta_found = left_match.found;
  probe.right_delta_found = right_match.found;
  probe.left_delta_index = left_match.index;
  probe.right_delta_index = right_match.index;
  return probe;
}

const std::vector<DirectObject>& GetDirectObjects() {
  static const std::vector<DirectObject> objects = [] {
    std::vector<DirectObject> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < kElementCount; ++i) {
      const auto seed = static_cast<std::uint64_t>(i + 1);
      values.push_back(
          DirectObject{seed, seed * 3u + 1u, seed * 5u + 7u, seed ^ 0x9e3779b97f4a7c15ull});
    }
    return values;
  }();
  return objects;
}

const std::vector<const DirectObject*>& GetDirectPointers() {
  static const std::vector<const DirectObject*> pointers = [] {
    const auto& objects = GetDirectObjects();
    std::vector<const DirectObject*> values;
    values.reserve(objects.size());
    for (const auto& object : objects) {
      values.push_back(&object);
    }
    return values;
  }();
  return pointers;
}

const std::vector<NonVirtualDiamondA>& GetNonVirtualObjects() {
  static const std::vector<NonVirtualDiamondA> objects = [] {
    std::vector<NonVirtualDiamondA> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < kElementCount; ++i) {
      values.emplace_back(static_cast<std::uint64_t>(i + 1));
    }
    return values;
  }();
  return objects;
}

const std::vector<const NonVirtualLeft*>& GetNonVirtualLeftPointers() {
  static const std::vector<const NonVirtualLeft*> pointers = [] {
    const auto& objects = GetNonVirtualObjects();
    std::vector<const NonVirtualLeft*> values;
    values.reserve(objects.size());
    for (const auto& object : objects) {
      values.push_back(&object);
    }
    return values;
  }();
  return pointers;
}

const std::vector<VirtualDiamondA>& GetVirtualMonoObjects() {
  static const std::vector<VirtualDiamondA> objects = [] {
    std::vector<VirtualDiamondA> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < kElementCount; ++i) {
      values.emplace_back(static_cast<std::uint64_t>(i + 1));
    }
    return values;
  }();
  return objects;
}

const std::vector<const VirtualDiamondA*>& GetVirtualCompletePointers() {
  static const std::vector<const VirtualDiamondA*> pointers = [] {
    const auto& objects = GetVirtualMonoObjects();
    std::vector<const VirtualDiamondA*> values;
    values.reserve(objects.size());
    for (const auto& object : objects) {
      values.push_back(&object);
    }
    return values;
  }();
  return pointers;
}

const std::vector<const VirtualLeft*>& GetVirtualLeftMonomorphicPointers() {
  static const std::vector<const VirtualLeft*> pointers = [] {
    const auto& objects = GetVirtualMonoObjects();
    std::vector<const VirtualLeft*> values;
    values.reserve(objects.size());
    for (const auto& object : objects) {
      values.push_back(&object);
    }
    return values;
  }();
  return pointers;
}

const std::vector<VirtualDiamondB>& GetVirtualPolyObjectsB() {
  static const std::vector<VirtualDiamondB> objects = [] {
    std::vector<VirtualDiamondB> values;
    values.reserve(kElementCount / 2);
    for (std::size_t i = 0; i < kElementCount / 2; ++i) {
      values.emplace_back(static_cast<std::uint64_t>(i + 7));
    }
    return values;
  }();
  return objects;
}

const std::vector<const VirtualLeft*>& GetVirtualLeftPolymorphicPointers() {
  static const std::vector<const VirtualLeft*> pointers = [] {
    const auto& a_objects = GetVirtualMonoObjects();
    const auto& b_objects = GetVirtualPolyObjectsB();
    std::vector<const VirtualLeft*> values;
    values.reserve(kElementCount);
    for (std::size_t i = 0; i < b_objects.size(); ++i) {
      values.push_back(&a_objects[i]);
      values.push_back(&b_objects[i]);
    }
    return values;
  }();
  return pointers;
}

BM_NOINLINE std::uint64_t ReadDirect(const DirectObject* object) {
  return object->shared + object->left_bias;
}

BM_NOINLINE std::uint64_t ReadNonVirtualLeft(const NonVirtualLeft* object) {
  return object->shared + object->left_bias;
}

BM_NOINLINE std::uint64_t ReadVirtualComplete(const VirtualDiamondA* object) {
  return object->shared + object->left_bias;
}

BM_NOINLINE std::uint64_t ReadVirtualLeft(const VirtualLeft* object) {
  return object->shared + object->left_bias;
}

void BM_VirtualBaseAbiModel(benchmark::State& state) {
  VirtualBaseAbiProbe probe{};
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t i = 0; i < kProbeRepeats; ++i) {
      probe = GetAbiProbe();
      checksum ^= static_cast<std::uint64_t>(probe.shared_from_left);
      checksum ^= static_cast<std::uint64_t>(probe.shared_from_right);
      checksum ^= static_cast<std::uint64_t>(probe.left_offset_to_top);
      checksum ^= static_cast<std::uint64_t>(probe.right_offset_to_top);
      checksum ^= probe.left_slot_result ^ probe.right_slot_result;
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.counters["direct_bytes"] = static_cast<double>(probe.direct_size);
  state.counters["non_virtual_bytes"] = static_cast<double>(probe.non_virtual_size);
  state.counters["virtual_a_bytes"] = static_cast<double>(probe.virtual_a_size);
  state.counters["virtual_b_bytes"] = static_cast<double>(probe.virtual_b_size);
  state.counters["left_subobject_bytes"] = static_cast<double>(probe.left_size);
  state.counters["right_subobject_bytes"] = static_cast<double>(probe.right_size);
  state.counters["shared_base_bytes"] = static_cast<double>(probe.shared_size);
  state.counters["left_offset"] = static_cast<double>(probe.left_offset);
  state.counters["right_offset"] = static_cast<double>(probe.right_offset);
  state.counters["shared_from_top"] = static_cast<double>(probe.shared_from_top);
  state.counters["shared_from_left"] = static_cast<double>(probe.shared_from_left);
  state.counters["shared_from_right"] = static_cast<double>(probe.shared_from_right);
  state.counters["shared_alias"] = static_cast<double>(probe.shared_alias);
  state.counters["left_offset_to_top"] =
      static_cast<double>(probe.left_offset_to_top);
  state.counters["right_offset_to_top"] =
      static_cast<double>(probe.right_offset_to_top);
  state.counters["shared_typeinfo"] = static_cast<double>(probe.shared_typeinfo);
  state.counters["left_slot0_result"] =
      static_cast<double>(probe.left_slot_result);
  state.counters["right_slot0_result"] =
      static_cast<double>(probe.right_slot_result);
  state.counters["left_vbase_delta_found"] =
      static_cast<double>(probe.left_delta_found);
  state.counters["right_vbase_delta_found"] =
      static_cast<double>(probe.right_delta_found);
  state.counters["left_vbase_delta_index"] =
      static_cast<double>(probe.left_delta_index);
  state.counters["right_vbase_delta_index"] =
      static_cast<double>(probe.right_delta_index);
  state.SetLabel("shared virtual base plus vtable-probed offset metadata on the local ABI");
}

template <typename PointerRange, typename Reader>
void RunPointerScan(benchmark::State& state,
                    const PointerRange& objects,
                    Reader reader,
                    std::size_t object_size) {
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    for (std::size_t repeat = 0; repeat < kOpsPerIteration; ++repeat) {
      for (const auto* object : objects) {
        checksum += reader(object);
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * kOpsPerIteration));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(objects.size() * object_size *
                                               kOpsPerIteration));
}

void BM_ReadDirectPointers(benchmark::State& state) {
  const auto& objects = GetDirectPointers();
  RunPointerScan(state, objects, ReadDirect, sizeof(DirectObject));
}

void BM_ReadNonVirtualBasePointers(benchmark::State& state) {
  const auto& objects = GetNonVirtualLeftPointers();
  RunPointerScan(state, objects, ReadNonVirtualLeft, sizeof(NonVirtualDiamondA));
}

void BM_ReadVirtualCompleteObject(benchmark::State& state) {
  const auto& objects = GetVirtualCompletePointers();
  RunPointerScan(state, objects, ReadVirtualComplete, sizeof(VirtualDiamondA));
}

void BM_ReadVirtualBaseMonomorphic(benchmark::State& state) {
  const auto& objects = GetVirtualLeftMonomorphicPointers();
  RunPointerScan(state, objects, ReadVirtualLeft, sizeof(VirtualDiamondA));
}

void BM_ReadVirtualBasePolymorphic(benchmark::State& state) {
  const auto& objects = GetVirtualLeftPolymorphicPointers();
  RunPointerScan(state, objects, ReadVirtualLeft, sizeof(VirtualDiamondA));
}

#undef BM_NOINLINE

}  // namespace

BENCHMARK(BM_VirtualBaseAbiModel)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_ReadDirectPointers)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReadNonVirtualBasePointers)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReadVirtualCompleteObject)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReadVirtualBaseMonomorphic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ReadVirtualBasePolymorphic)->Unit(benchmark::kMillisecond);
