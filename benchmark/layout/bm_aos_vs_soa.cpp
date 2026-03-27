#include <benchmark/benchmark.h>

#include <cstddef>
#include <vector>

namespace {

constexpr std::size_t kCount = 16 * 1024 * 1024;

struct Particle {
  float x;
  float y;
  float z;
  float w;
};

struct WideParticle {
  float v[16];
};

void BM_AoS(benchmark::State& state) {
  std::vector<Particle> particles(kCount, Particle{1.0f, 2.0f, 3.0f, 4.0f});
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < particles.size(); ++i) {
      sum += particles[i].x + particles[i].y;
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(particles.size()));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(particles.size() * sizeof(Particle)));
}

void BM_SoA(benchmark::State& state) {
  std::vector<float> x(kCount, 1.0f);
  std::vector<float> y(kCount, 2.0f);
  std::vector<float> z(kCount, 3.0f);
  std::vector<float> w(kCount, 4.0f);
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < x.size(); ++i) {
      sum += x[i] + y[i];
    }
    benchmark::DoNotOptimize(sum);
  }

  benchmark::DoNotOptimize(z.data());
  benchmark::DoNotOptimize(w.data());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(x.size()));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(x.size() * sizeof(float) * 2));
}

void BM_AoS_OneField(benchmark::State& state) {
  std::vector<Particle> particles(kCount, Particle{1.0f, 2.0f, 3.0f, 4.0f});
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < particles.size(); ++i) {
      sum += particles[i].x;
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(particles.size()));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(particles.size() * sizeof(Particle)));
}

void BM_SoA_OneField(benchmark::State& state) {
  std::vector<float> x(kCount, 1.0f);
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < x.size(); ++i) {
      sum += x[i];
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(x.size()));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(x.size() * sizeof(float)));
}

void BM_AoS_WideTwoUsed(benchmark::State& state) {
  std::vector<WideParticle> particles(kCount);
  for (auto& p : particles) {
    for (int i = 0; i < 16; ++i) {
      p.v[i] = static_cast<float>(i + 1);
    }
  }
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < particles.size(); ++i) {
      sum += particles[i].v[0] + particles[i].v[1];
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(particles.size()));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(particles.size() * sizeof(WideParticle)));
}

void BM_SoA_WideTwoUsed(benchmark::State& state) {
  std::vector<float> f0(kCount, 1.0f);
  std::vector<float> f1(kCount, 2.0f);
  std::vector<float> cold[14];
  for (auto& v : cold) {
    v.assign(kCount, 3.0f);
  }
  float sum = 0.0f;

  for (auto _ : state) {
    for (std::size_t i = 0; i < f0.size(); ++i) {
      sum += f0[i] + f1[i];
    }
    benchmark::DoNotOptimize(sum);
  }

  benchmark::DoNotOptimize(cold[0].data());
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(f0.size()));
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(f0.size() * sizeof(float) * 2));
}

}  // namespace

BENCHMARK(BM_AoS)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SoA)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_AoS_OneField)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SoA_OneField)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_AoS_WideTwoUsed)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SoA_WideTwoUsed)->Unit(benchmark::kMicrosecond);
