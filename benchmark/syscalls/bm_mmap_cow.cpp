#include <benchmark/benchmark.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t kMapBytes = 64 * 1024 * 1024;
constexpr std::size_t kPageBytes = 4096;

class TempFile {
 public:
  TempFile() {
    std::array<char, 64> path{};
    std::snprintf(path.data(), path.size(), "/tmp/cpp_perf_lab_cow_%dXXXXXX", ::getpid());
    fd_ = ::mkstemp(path.data());
    if (fd_ < 0) {
      throw std::runtime_error("mkstemp failed");
    }
    path_ = path.data();
    if (::ftruncate(fd_, static_cast<off_t>(kMapBytes)) != 0) {
      throw std::runtime_error("ftruncate failed");
    }
  }

  ~TempFile() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
    if (!path_.empty()) {
      ::unlink(path_.c_str());
    }
  }

  int fd() const { return fd_; }

 private:
  int fd_ = -1;
  std::string path_;
};

TempFile& GetTempFile() {
  static TempFile file;
  return file;
}

std::uint64_t WritePass(std::uint8_t* bytes, std::uint8_t seed) {
  std::uint64_t checksum = 0;
  for (std::size_t offset = 0; offset < kMapBytes; offset += kPageBytes) {
    bytes[offset] = static_cast<std::uint8_t>(seed + ((offset / kPageBytes) & 0xFFu));
    checksum += bytes[offset];
  }
  return checksum;
}

void RunFirstTouchBenchmark(benchmark::State& state, int flags, bool sync_back) {
  TempFile& file = GetTempFile();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    void* region = ::mmap(nullptr, kMapBytes, PROT_READ | PROT_WRITE, flags, file.fd(), 0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }

    checksum += WritePass(static_cast<std::uint8_t*>(region), 1u);
    if (sync_back && ::msync(region, kMapBytes, MS_SYNC) != 0) {
      ::munmap(region, kMapBytes);
      state.SkipWithError("msync failed");
      return;
    }
    ::munmap(region, kMapBytes);
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kMapBytes));
}

void RunRewriteBenchmark(benchmark::State& state, int flags, bool sync_back) {
  TempFile& file = GetTempFile();
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    void* region = ::mmap(nullptr, kMapBytes, PROT_READ | PROT_WRITE, flags, file.fd(), 0);
    if (region == MAP_FAILED) {
      state.SkipWithError("mmap failed");
      return;
    }

    auto* bytes = static_cast<std::uint8_t*>(region);
    state.PauseTiming();
    checksum += WritePass(bytes, 1u);
    if (sync_back && ::msync(region, kMapBytes, MS_SYNC) != 0) {
      ::munmap(region, kMapBytes);
      state.SkipWithError("msync failed");
      return;
    }
    state.ResumeTiming();
    checksum += WritePass(bytes, 17u);
    if (sync_back && ::msync(region, kMapBytes, MS_SYNC) != 0) {
      ::munmap(region, kMapBytes);
      state.SkipWithError("msync failed");
      return;
    }
    state.PauseTiming();
    ::munmap(region, kMapBytes);
    state.ResumeTiming();
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kMapBytes));
}

void BM_MmapPrivateFirstTouch(benchmark::State& state) {
  RunFirstTouchBenchmark(state, MAP_PRIVATE, false);
}

void BM_MmapPrivateRewrite(benchmark::State& state) {
  RunRewriteBenchmark(state, MAP_PRIVATE, false);
}

void BM_MmapSharedWrite(benchmark::State& state) {
  RunFirstTouchBenchmark(state, MAP_SHARED, false);
}

void BM_MmapSharedWriteMsync(benchmark::State& state) {
  RunFirstTouchBenchmark(state, MAP_SHARED, true);
}

}  // namespace

BENCHMARK(BM_MmapPrivateFirstTouch)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MmapPrivateRewrite)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MmapSharedWrite)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MmapSharedWriteMsync)->Unit(benchmark::kMillisecond);
