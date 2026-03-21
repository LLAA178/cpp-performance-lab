# Results Summary

## Environment

- Date: 2026-03-21
- Machine / CPU: Apple Silicon (8 cores reported by benchmark)
- OS: macOS (Darwin 25.x)
- Compiler: AppleClang (C++20)
- Build flags: Release, `-O3 -march=native`
- Command: `scripts/run_all.sh`

## 1) Stride Access

- Key numbers: stride 1/4/16/64 -> `3.49G / 3.33G / 1.01G / 0.346G items/s`
- Observation: throughput drops significantly as stride increases.
- Conclusion: reduced spatial locality makes memory access more latency-bound.
- Key takeaway: sequential access is cache-efficient; large strides cause major throughput loss.

## 2) Pointer Chasing

- Key numbers: sequential `16.7G items/s` vs pointer chasing `12.8M items/s`
- Observation: random pointer chasing is roughly three orders of magnitude slower.
- Conclusion: hardware prefetching is ineffective and latency dominates.
- Key takeaway: irregular access patterns can overwhelm any compute-side optimization.

## 3) False Sharing

- Key numbers: adjacent `155.7M` vs padded `509.4M items/s`
- Observation: `alignas(64)` increases throughput by about 3.3x.
- Conclusion: independent logical writes on the same cache line cause heavy coherence traffic.
- Key takeaway: cache-line ownership, not variable ownership, controls write scalability.

## 4) AoS vs SoA

- Key numbers:
  - Base case: AoS `1.318G` vs SoA `1.303G items/s` (close)
  - Wide-struct, two fields used: AoS `0.983G` vs SoA `1.396G items/s`
- Observation: SoA advantage becomes clear when only a subset of fields is used.
- Conclusion: SoA improves effective bandwidth when field utilization is sparse.
- Key takeaway: layout choice should follow access density, not a fixed rule.

## 5) Mutex vs Atomic

- Key numbers:
  - Atomic 1/2/4 threads: `571.9M / 163.3M / 44.9M`
  - Mutex 1/2/4 threads: `221.0M / 62.7M / 6.67M items/s`
- Observation: both degrade under contention; mutex degrades faster.
- Conclusion: both pay coherence costs, and mutex adds lock/unlock overhead.
- Key takeaway: contention on shared-write state dominates synchronization choice.

## 6) Cache Levels

- Key numbers: 4KB `1.01G` -> 256KB `280M` -> 1MB `206M` -> 16MB `88M` -> 64MB `12.7M items/s`
- Observation: clear multi-stage drops as working set increases.
- Conclusion: miss cost progressively dominates when crossing cache hierarchy boundaries.
- Key takeaway: working-set size is a primary performance control variable.

## 7) ILP

- Key numbers: dependent `670M` vs independent `2.30G items/s`
- Observation: independent streams provide about 3.4x higher throughput.
- Conclusion: dependency chains limit out-of-order overlap and instruction-level parallelism.
- Key takeaway: reducing dependencies can deliver larger gains than micro-tuning instructions.

## 8) Cache Associativity

- Key numbers:
  - Friendly lines=64: `928M items/s`
  - Conflict lines=64: `254M items/s`
  - Sharp drop appears at conflict lines=32/64
- Observation: conflict-stride access collapses throughput beyond a threshold.
- Conclusion: this is conflict-miss behavior (associativity overflow).
- Key takeaway: sufficient cache capacity does not prevent set-mapping conflict penalties.

## 9) Queue

- Key numbers (aggregate mean):
  - Mutex (`batch=64, backoff=0`): `111.6M ops/s`
  - SPSC (`batch=8, backoff=0`): `174.1M ops/s`
- Observation: with tuned parameters, SPSC outperforms mutex queue.
- Conclusion: in 1P1C transfer, ring buffers can reduce lock contention and critical-section cost.
- Key takeaway: queue performance is highly sensitive to backoff and batching policy.

## 10) Memory Pool

- Key numbers:
  - `new/delete`: `109.3M ops/s`
  - locked pool: `21.3M ops/s`
  - thread-local pool: `103.1M ops/s`
- Observation: shared locked pool is the slowest; thread-local pool is close to allocator baseline.
- Conclusion: pooling can underperform when global synchronization cost is high.
- Key takeaway: allocator strategy should prioritize thread locality first.

## Final Takeaways

- Cache/locality: stride, pointer chasing, and associativity all show that access pattern sets the upper bound.
- Latency vs throughput: regular sequential access is throughput-friendly; random/conflicting access is latency-dominated.
- Contention/synchronization: false sharing, mutex/atomic, and queue tests all expose shared-write hotspots.
- Data layout: AoS vs SoA should be decided by field utilization and vectorization opportunities.
- Allocation strategy: memory pools need thread-aware design, otherwise synchronization overhead can erase gains.
