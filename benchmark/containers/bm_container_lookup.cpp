#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kLookupCount = 1 << 20;

struct QuerySet {
  std::vector<std::uint32_t> keys;
  std::vector<std::uint32_t> queries;
};

QuerySet MakeQuerySet(std::size_t entry_count, bool mixed_miss) {
  QuerySet out;
  out.keys.resize(entry_count);
  for (std::size_t i = 0; i < out.keys.size(); ++i) {
    out.keys[i] = static_cast<std::uint32_t>((i * 2654435761u) ^ 0x9e3779b9u);
  }

  out.queries.resize(kLookupCount);
  for (std::size_t i = 0; i < out.queries.size(); ++i) {
    if (mixed_miss && (i & 1u)) {
      out.queries[i] = out.keys[i & (out.keys.size() - 1)] ^ 0x7f4a7c15u;
    } else {
      out.queries[i] = out.keys[i & (out.keys.size() - 1)];
    }
  }
  return out;
}

const QuerySet& GetSmallHotSet() {
  static const QuerySet set = MakeQuerySet(256, false);
  return set;
}

const QuerySet& GetLargeMixedSet() {
  static const QuerySet set = MakeQuerySet(1 << 16, true);
  return set;
}

template <class LookupFn>
void RunLookupBenchmark(benchmark::State& state, const QuerySet& set, LookupFn lookup_fn) {
  std::uint64_t checksum = 0;
  std::size_t hits = 0;

  for (auto _ : state) {
    for (std::uint32_t key : set.queries) {
      std::uint32_t value = 0;
      if (lookup_fn(key, value)) {
        checksum += value;
        ++hits;
      }
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.counters["hit_rate"] =
      static_cast<double>(hits) / static_cast<double>(state.iterations() * set.queries.size());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(set.queries.size()));
}

template <const QuerySet& (*GetSet)()>
void BM_MapLookup(benchmark::State& state) {
  const auto& set = GetSet();
  std::map<std::uint32_t, std::uint32_t> map;
  for (std::size_t i = 0; i < set.keys.size(); ++i) {
    map.emplace(set.keys[i], static_cast<std::uint32_t>(i));
  }

  RunLookupBenchmark(state, set, [&](std::uint32_t key, std::uint32_t& value) {
    auto it = map.find(key);
    if (it == map.end()) {
      return false;
    }
    value = it->second;
    return true;
  });
}

template <const QuerySet& (*GetSet)()>
void BM_UnorderedMapLookup(benchmark::State& state) {
  const auto& set = GetSet();
  std::unordered_map<std::uint32_t, std::uint32_t> map;
  map.reserve(set.keys.size());
  for (std::size_t i = 0; i < set.keys.size(); ++i) {
    map.emplace(set.keys[i], static_cast<std::uint32_t>(i));
  }

  RunLookupBenchmark(state, set, [&](std::uint32_t key, std::uint32_t& value) {
    auto it = map.find(key);
    if (it == map.end()) {
      return false;
    }
    value = it->second;
    return true;
  });
}

template <const QuerySet& (*GetSet)()>
void BM_SortedVectorLookup(benchmark::State& state) {
  const auto& set = GetSet();
  std::vector<std::pair<std::uint32_t, std::uint32_t>> entries;
  entries.reserve(set.keys.size());
  for (std::size_t i = 0; i < set.keys.size(); ++i) {
    entries.emplace_back(set.keys[i], static_cast<std::uint32_t>(i));
  }
  std::sort(entries.begin(), entries.end());

  RunLookupBenchmark(state, set, [&](std::uint32_t key, std::uint32_t& value) {
    auto it = std::lower_bound(entries.begin(),
                               entries.end(),
                               std::pair<std::uint32_t, std::uint32_t>{key, 0u});
    if (it == entries.end() || it->first != key) {
      return false;
    }
    value = it->second;
    return true;
  });
}

}  // namespace

BENCHMARK(BM_MapLookup<GetSmallHotSet>)->Name("BM_MapLookupSmallHot")->Unit(benchmark::kMillisecond);
BENCHMARK(BM_UnorderedMapLookup<GetSmallHotSet>)
    ->Name("BM_UnorderedMapLookupSmallHot")
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SortedVectorLookup<GetSmallHotSet>)
    ->Name("BM_SortedVectorLookupSmallHot")
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MapLookup<GetLargeMixedSet>)->Name("BM_MapLookupLargeMixed")->Unit(benchmark::kMillisecond);
BENCHMARK(BM_UnorderedMapLookup<GetLargeMixedSet>)
    ->Name("BM_UnorderedMapLookupLargeMixed")
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SortedVectorLookup<GetLargeMixedSet>)
    ->Name("BM_SortedVectorLookupLargeMixed")
    ->Unit(benchmark::kMillisecond);
