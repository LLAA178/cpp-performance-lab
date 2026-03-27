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
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kFileBytes = 64 * 1024 * 1024;
constexpr std::size_t kChunkBytes = 4 * 1024;

class TempFile {
 public:
  TempFile() {
    std::array<char, 64> path{};
    std::snprintf(path.data(), path.size(), "/tmp/cpp_perf_lab_mmap_%dXXXXXX", ::getpid());
    fd_ = ::mkstemp(path.data());
    if (fd_ < 0) {
      throw std::runtime_error("mkstemp failed");
    }
    path_ = path.data();
    if (::ftruncate(fd_, static_cast<off_t>(kFileBytes)) != 0) {
      throw std::runtime_error("ftruncate failed");
    }

    std::vector<std::uint8_t> buf(kChunkBytes);
    for (std::size_t i = 0; i < buf.size(); ++i) {
      buf[i] = static_cast<std::uint8_t>((i * 131u) & 0xFFu);
    }
    for (std::size_t offset = 0; offset < kFileBytes; offset += buf.size()) {
      const ssize_t written = ::pwrite(fd_, buf.data(), buf.size(), static_cast<off_t>(offset));
      if (written != static_cast<ssize_t>(buf.size())) {
        throw std::runtime_error("pwrite failed");
      }
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

void BM_ReadSequential(benchmark::State& state) {
  TempFile& file = GetTempFile();
  std::vector<std::uint8_t> buf(kChunkBytes);
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    if (::lseek(file.fd(), 0, SEEK_SET) < 0) {
      state.SkipWithError("lseek failed");
      break;
    }
    for (std::size_t offset = 0; offset < kFileBytes; offset += buf.size()) {
      const ssize_t n = ::read(file.fd(), buf.data(), buf.size());
      if (n != static_cast<ssize_t>(buf.size())) {
        state.SkipWithError("read failed");
        return;
      }
      checksum += buf[0];
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kFileBytes));
  state.counters["chunk_kib"] = static_cast<double>(kChunkBytes / 1024);
}

void BM_PreadRandom(benchmark::State& state) {
  TempFile& file = GetTempFile();
  std::vector<std::uint8_t> buf(kChunkBytes);
  const std::size_t page_count = kFileBytes / kChunkBytes;
  std::uint64_t checksum = 0;

  for (auto _ : state) {
    std::uint64_t lcg = 0x9e3779b97f4a7c15ULL;
    for (std::size_t i = 0; i < page_count; ++i) {
      lcg = lcg * 2862933555777941757ULL + 3037000493ULL;
      const std::size_t page = static_cast<std::size_t>(lcg % page_count);
      const off_t offset = static_cast<off_t>(page * kChunkBytes);
      const ssize_t n = ::pread(file.fd(), buf.data(), buf.size(), offset);
      if (n != static_cast<ssize_t>(buf.size())) {
        state.SkipWithError("pread failed");
        return;
      }
      checksum += buf[0];
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kFileBytes));
  state.counters["chunk_kib"] = static_cast<double>(kChunkBytes / 1024);
}

void BM_MmapSequential(benchmark::State& state) {
  TempFile& file = GetTempFile();
  void* mapped = ::mmap(nullptr, kFileBytes, PROT_READ, MAP_PRIVATE, file.fd(), 0);
  if (mapped == MAP_FAILED) {
    state.SkipWithError("mmap failed");
    return;
  }

  auto* bytes = static_cast<const std::uint8_t*>(mapped);
  std::uint64_t checksum = 0;
  for (auto _ : state) {
    for (std::size_t offset = 0; offset < kFileBytes; offset += kChunkBytes) {
      checksum += bytes[offset];
    }
    benchmark::ClobberMemory();
  }

  benchmark::DoNotOptimize(checksum);
  ::munmap(mapped, kFileBytes);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kFileBytes));
  state.counters["chunk_kib"] = static_cast<double>(kChunkBytes / 1024);
}

}  // namespace

BENCHMARK(BM_ReadSequential)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PreadRandom)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_MmapSequential)->Unit(benchmark::kMillisecond);
