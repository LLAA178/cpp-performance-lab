#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr std::size_t kItemCount = 1 << 20;

enum class Kind : std::uint8_t {
  kAdd,
  kMul,
};

struct TaggedOp {
  Kind kind;
  std::uint64_t value;
};

struct BaseOp {
  virtual ~BaseOp() = default;
};

struct AddOp final : BaseOp {
  std::uint64_t value;
};

struct MulOp final : BaseOp {
  std::uint64_t value;
};

struct DispatchSet {
  std::vector<TaggedOp> tagged;
  std::vector<AddOp> adds;
  std::vector<MulOp> muls;
  std::vector<BaseOp*> polymorphic;
};

DispatchSet MakeDispatchSet() {
  DispatchSet set;
  set.tagged.reserve(kItemCount);
  set.adds.reserve(kItemCount / 2);
  set.muls.reserve(kItemCount / 2);
  set.polymorphic.reserve(kItemCount);

  for (std::size_t i = 0; i < kItemCount; ++i) {
    if ((i & 1u) == 0u) {
      set.tagged.push_back(TaggedOp{Kind::kAdd, static_cast<std::uint64_t>((i & 15u) + 1u)});
      AddOp add;
      add.value = static_cast<std::uint64_t>((i & 15u) + 1u);
      set.adds.push_back(add);
      set.polymorphic.push_back(&set.adds.back());
    } else {
      set.tagged.push_back(TaggedOp{Kind::kMul, static_cast<std::uint64_t>((i & 3u) + 2u)});
      MulOp mul;
      mul.value = static_cast<std::uint64_t>((i & 3u) + 2u);
      set.muls.push_back(mul);
      set.polymorphic.push_back(&set.muls.back());
    }
  }

  return set;
}

const DispatchSet& GetDispatchSet() {
  static const DispatchSet set = MakeDispatchSet();
  return set;
}

void BM_TagDispatch(benchmark::State& state) {
  const auto& set = GetDispatchSet();
  std::uint64_t acc = 1;
  for (auto _ : state) {
    for (const TaggedOp& op : set.tagged) {
      if (op.kind == Kind::kAdd) {
        acc += op.value;
      } else {
        acc *= op.value;
      }
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(acc);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(set.tagged.size()));
}

void BM_DynamicCastDispatch(benchmark::State& state) {
  const auto& set = GetDispatchSet();
  std::uint64_t acc = 1;
  for (auto _ : state) {
    for (BaseOp* op : set.polymorphic) {
      if (const auto* add = dynamic_cast<AddOp*>(op)) {
        acc += add->value;
      } else {
        const auto* mul = dynamic_cast<MulOp*>(op);
        acc *= mul->value;
      }
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(acc);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(set.polymorphic.size()));
}

}  // namespace

BENCHMARK(BM_TagDispatch)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_DynamicCastDispatch)->Unit(benchmark::kMillisecond);
