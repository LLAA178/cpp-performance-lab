#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace {

constexpr std::size_t kItemCount = 1 << 20;

struct Add {
  std::uint64_t a;
};

struct Mul {
  std::uint64_t m;
};

using OpVariant = std::variant<Add, Mul>;

struct VirtualOp {
  virtual ~VirtualOp() = default;
  virtual std::uint64_t apply(std::uint64_t x) const = 0;
};

struct AddOp final : VirtualOp {
  std::uint64_t a;
  std::uint64_t apply(std::uint64_t x) const override { return x + a; }
};

struct MulOp final : VirtualOp {
  std::uint64_t m;
  std::uint64_t apply(std::uint64_t x) const override { return x * m; }
};

struct DispatchSet {
  std::vector<OpVariant> variants;
  std::vector<AddOp> adds;
  std::vector<MulOp> muls;
  std::vector<const VirtualOp*> virtuals;
};

DispatchSet MakeDispatchSet() {
  DispatchSet set;
  set.variants.reserve(kItemCount);
  set.adds.reserve(kItemCount / 2);
  set.muls.reserve(kItemCount / 2);
  set.virtuals.reserve(kItemCount);

  for (std::size_t i = 0; i < kItemCount; ++i) {
    if ((i & 1u) == 0u) {
      AddOp op;
      op.a = static_cast<std::uint64_t>((i & 15u) + 1u);
      set.variants.emplace_back(Add{op.a});
      set.adds.push_back(op);
      set.virtuals.push_back(&set.adds.back());
    } else {
      MulOp op;
      op.m = static_cast<std::uint64_t>((i & 3u) + 2u);
      set.variants.emplace_back(Mul{op.m});
      set.muls.push_back(op);
      set.virtuals.push_back(&set.muls.back());
    }
  }
  return set;
}

const DispatchSet& GetDispatchSet() {
  static const DispatchSet set = MakeDispatchSet();
  return set;
}

void BM_VariantDispatch(benchmark::State& state) {
  const auto& set = GetDispatchSet();
  std::uint64_t acc = 1;
  for (auto _ : state) {
    for (const OpVariant& op : set.variants) {
      acc = std::visit(
          [&](const auto& impl) -> std::uint64_t {
            using T = std::decay_t<decltype(impl)>;
            if constexpr (std::is_same_v<T, Add>) {
              return acc + impl.a;
            } else {
              return acc * impl.m;
            }
          },
          op);
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(acc);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(set.variants.size()));
}

void BM_VirtualHierarchy(benchmark::State& state) {
  const auto& set = GetDispatchSet();
  std::uint64_t acc = 1;
  for (auto _ : state) {
    for (const VirtualOp* op : set.virtuals) {
      acc = op->apply(acc);
    }
    benchmark::ClobberMemory();
  }
  benchmark::DoNotOptimize(acc);
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(set.virtuals.size()));
}

}  // namespace

BENCHMARK(BM_VariantDispatch)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_VirtualHierarchy)->Unit(benchmark::kMillisecond);
