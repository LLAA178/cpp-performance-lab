# Results Summary

## Environment

- Date: 2026-03-21
- Additional benchmark runs: 2026-03-26
- Machine / CPU: Apple Silicon (8 cores reported by benchmark)
- OS: macOS (Darwin 25.x)
- Compiler: AppleClang (C++20)
- Build flags: Release, `-O3 -march=native`
- Command: `scripts/run_all.sh` plus targeted runs for new benchmarks

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

## 8) Branch Predictability

- Key numbers:
- always-taken branch: `7.49G items/s`
- alternating branch: `7.55G items/s`
- pseudo-random branch: `7.56G items/s`
- branchless pseudo-random: `7.88G items/s`
- Observation: all four variants are close on this Apple Silicon and AppleClang `-O3` run, with branchless only modestly ahead.
- Conclusion: this code shape does not expose a large branch-prediction penalty on the local platform, so the valid result here is a narrow spread rather than a dramatic textbook gap.
- Key takeaway: if the branch and branchless forms converge, the right conclusion is "no strong signal here", not "branch prediction never matters".

## 9) Inlining Effects

- Key numbers:
- forced inline: `1.22G items/s`
- forced noinline: `1.23G items/s`
- function pointer: `1.24G items/s`
- Observation: the three call shapes land essentially on top of each other in this tight arithmetic chain benchmark.
- Conclusion: with this compiler and this code shape, forced inline vs noinline does not produce a meaningful standalone throughput difference; the stronger dispatch benchmarks remain the more useful signal.
- Key takeaway: when inlining results stay close, the repo still keeps the benchmark as a local conclusion rather than deleting the topic entirely.

## 10) Cache Associativity

- Key numbers:
  - Friendly lines=64: `928M items/s`
  - Conflict lines=64: `254M items/s`
  - Sharp drop appears at conflict lines=32/64
- Observation: conflict-stride access collapses throughput beyond a threshold.
- Conclusion: this is conflict-miss behavior (associativity overflow).
- Key takeaway: sufficient cache capacity does not prevent set-mapping conflict penalties.

## 11) Queue

- Key numbers (aggregate mean):
  - Mutex (`batch=64, backoff=0`): `111.6M ops/s`
  - SPSC (`batch=8, backoff=0`): `174.1M ops/s`
- Observation: with tuned parameters, SPSC outperforms mutex queue.
- Conclusion: in 1P1C transfer, ring buffers can reduce lock contention and critical-section cost.
- Key takeaway: queue performance is highly sensitive to backoff and batching policy.

## 12) Memory Pool

- Key numbers:
  - `new/delete`: `109.3M ops/s`
  - locked pool: `21.3M ops/s`
  - thread-local pool: `103.1M ops/s`
- Observation: shared locked pool is the slowest; thread-local pool is close to allocator baseline.
- Conclusion: pooling can underperform when global synchronization cost is high.
- Key takeaway: allocator strategy should prioritize thread locality first.

## 13) `mmap` vs `read` / `pread`

- Key numbers:
  - sequential `read`: `6.70 GiB/s`
  - random `pread`: `5.51 GiB/s`
  - sequential `mmap`: `676.4 GiB/s`
- Observation: mapped-file scanning is dramatically faster than syscall-based reads on the warm-cache path measured here.
- Conclusion: once mapping is established, direct memory access removes per-chunk syscall overhead; random `pread` remains slower because access locality is weaker.
- Key takeaway: `mmap` is a strong baseline for repeated warm reads, but interpretation must account for page-cache state and first-touch fault cost.

## 14) Memory Order

- Key numbers:
  - latest rerun:
    `fetch_add` 1 thread: relaxed `527.5M`, acq_rel `527.5M`, seq_cst `528.1M items/s`
    `fetch_add` 4 threads: relaxed `25.9M`, acq_rel `38.9M`, seq_cst `24.0M items/s`
  - flag handoff: acq_rel `25.9M` vs seq_cst `26.4M handoffs/s`
  - publish/consume: release-acquire `13.3M` vs seq_cst `13.3M publishes/s`
  - SPSC ring metadata: acq_rel `30.9M` vs seq_cst `23.9M ring ops/s`
  - message passing litmus:
    relaxed bad reads mean `1.4 / 100k`
    release-acquire bad reads mean `0 / 100k`
  - store-buffering litmus:
    relaxed both-zero mean `96.9k / 100k`
    release-acquire both-zero mean `96.7k / 100k`
    seq_cst both-zero mean `0 / 100k`
- Observation: the 4-thread `fetch_add` case is dominated by cache-line contention, so `relaxed` vs acq_rel is noisy there, but the correctness litmus tests show the semantic differences cleanly.
- Conclusion: `relaxed` is not safe for publication patterns, release/acquire fixes single-variable message passing, and release/acquire is still weaker than `seq_cst` when you need a single global order across multiple atomics.
- Key takeaway: use throughput tests to measure cost, but use litmus tests to show what can actually go wrong with weaker memory orders.

## 15) Thread Placement

- Key numbers:
  - default transfer: `29.9M ops/s`
  - shared-placement hint: `30.7M ops/s`
  - split-placement hint: `30.6M ops/s`
  - placement requests: `0`
  - placement verified: `0`
- Observation: the tightened benchmark now reports whether a placement request was actually issued and verified; on this macOS run, neither happened.
- Conclusion: this machine/runtime combination is not honoring the placement path used by the benchmark, so the three variants should be interpreted as the same baseline.
- Key takeaway: placement experiments must self-report whether the OS accepted the requested policy, otherwise the benchmark can look valid while measuring nothing.

## 16) Pipe vs Shared-Memory Handoff

- Key numbers:
  - pipe handoff: `2.31M msgs/s`
  - shared-memory mailbox: `10.6M msgs/s`
- Observation: the shared-memory mailbox is about 4.6x faster than the pipe path in this thread-to-thread handoff test.
- Conclusion: syscall-heavy message transfer pays a large fixed cost relative to a lock-free shared-memory handoff.
- Key takeaway: for small messages and tight loops, avoiding kernel crossings can materially improve throughput.

## 17) Dispatch Cost

- Key numbers:
  - template dispatch: `3.10G items/s`
  - function pointer: `2.95G items/s`
  - virtual dispatch: `1.14G items/s`
- Observation: template and function-pointer dispatch are close, while virtual dispatch is about 2.7x slower in this tight loop.
- Conclusion: when the compiler can keep the call path simple, compile-time or direct-call forms preserve much higher throughput than virtual dispatch.
- Key takeaway: polymorphism choice can materially affect hot-loop throughput, especially when the per-item work is small.

## 18) Callable Abstraction

- Key numbers:
  - lambda: `3.57G items/s`
  - functor: `3.60G items/s`
  - function pointer: `3.63G items/s`
  - `std::function`: `1.23G items/s`
- Observation: erased callable dispatch through `std::function` is roughly 3x slower than the other forms measured here.
- Conclusion: lightweight callable abstractions remain near direct-call speed, while type erasure introduces a visible hot-path cost.
- Key takeaway: `std::function` is convenient, but it should not be the default in throughput-critical inner loops.

## 19) Clock Call Overhead

- Key numbers:
  - `steady_clock::now()`: `70.1M calls/s`
  - `system_clock::now()`: `65.6M calls/s`
  - `clock_gettime`: `73.9M calls/s`
  - `gettimeofday`: `98.2M calls/s`
- Observation: all four timing APIs are in the same rough cost band, with `gettimeofday` fastest in this run.
- Conclusion: time-source selection still matters in very tight loops, but the gap is tens of millions of calls per second rather than orders of magnitude.
- Key takeaway: timestamping is not free; measure the exact clock path used by a latency-sensitive loop.

## 20) MPSC and MPMC Queue Scaling

- Key numbers:
  - MPSC mutex queue: `8.10M msgs/s`
  - MPSC bounded MPMC queue: `9.55M msgs/s`
  - MPMC mutex queue: `21.2M msgs/s`
  - MPMC bounded MPMC queue: `7.46M msgs/s`
- Observation: the bounded lock-free queue is modestly faster in the 4P1C case, but the mutex queue is much faster in the 4P4C run on this machine.
- Conclusion: queue algorithm choice remains workload- and implementation-sensitive; lock-free does not imply universally higher throughput.
- Key takeaway: match queue design to the actual producer-consumer topology instead of assuming one structure wins everywhere.

## 21) Blocking vs Spinning

- Key numbers:
  - spin handoff: `17.5M handoffs/s`
  - yield handoff: `253.8k handoffs/s`
  - condition variable handoff: `107.2k handoffs/s`
- Observation: busy spinning is orders of magnitude faster than yielding or blocking in this tight ping-pong benchmark.
- Conclusion: when both sides stay active and handoffs are frequent, scheduler-mediated wakeups dominate the cost.
- Key takeaway: blocking primitives save CPU, but for extremely hot handoff loops they can impose a large throughput penalty.

## 22) TLB Pressure

- Key numbers:
  - contiguous page walk: `121.5M items/s`
  - page-stride walk: `873.8M items/s`
  - random page walk: `671.2M items/s`
- Observation: randomized page traversal is materially slower than deterministic page-stride access.
- Conclusion: once a benchmark is dominated by page-level access, TLB and page-walk behavior become visible even when every access touches only one value per page.
- Key takeaway: page access order matters; page-locality loss can reduce throughput well before bandwidth is saturated.

## 23) Exception vs Error Code

- Key numbers:
  - error-code no-fail: `3.50G items/s`
  - exception no-fail: `2.66G items/s`
  - error-code rare-fail: `2.46G items/s`
  - exception rare-fail: `398M items/s`
- Observation: the no-throw exception path is somewhat slower than optional-style signaling, and actual throws are dramatically slower even at low failure frequency.
- Conclusion: exception handling changes the cost model sharply once failures occur.
- Key takeaway: exceptions can be acceptable on cold paths, but they are expensive for frequently evaluated or moderately hot error paths.

## 24) Lock Variants

- Key numbers:
  - work=1, 1 thread: mutex `205M`, spinlock `575M`, ticket lock `528M items/s`
  - work=1, 4 threads: mutex `26.7M`, spinlock `3.18M`, ticket lock `4.33M items/s`
  - work=32, 1 thread: mutex `37.7M`, spinlock `39.6M`, ticket lock `39.7M items/s`
  - work=32, 4 threads: mutex `4.77M`, spinlock `2.48M`, ticket lock `2.69M items/s`
- Observation: once the critical section becomes larger, the uncontended advantage of spin and ticket locks mostly disappears, while their contended behavior remains poor on this machine.
- Conclusion: critical-section size matters as much as lock algorithm choice.
- Key takeaway: benchmark lock variants under both tiny and non-trivial work inside the lock; uncontended lock speed alone is not enough.

## 25) `mmap` Private vs Shared Writes

- Key numbers:
  - private first-touch write: `7.27 GiB/s`
  - private rewrite on already-dirtied pages: `48.5 GiB/s`
  - shared write without `msync`: `26.2 GiB/s`
  - shared write with `msync`: `11.9 GiB/s`
- Observation: first-touch private writes are far slower than rewriting already-private pages, and forcing `msync` cuts shared-write throughput sharply.
- Conclusion: copy-on-write fault cost, dirty-page state, and flush policy all matter enough that a single mapped-write benchmark is too coarse.
- Key takeaway: mapped-write benchmarks should separate first-touch, steady-state rewrite, and explicit durability cost.

## 26) Pipe vs `socketpair`

- Key numbers:
  - pipe: `2.43M msgs/s`
  - Unix stream `socketpair`: `1.42M msgs/s`
- Observation: the pipe path is still clearly faster than the Unix-domain stream socket pair after tightening the benchmark to the reliable stream case.
- Conclusion: even same-host kernel communication paths have a measurable abstraction ladder.
- Key takeaway: prefer the narrowest IPC primitive that matches the dataflow and semantics you need.

## 27) Cross-Thread Free

- Key numbers:
  - refined rerun:
    `new/delete` `20.8M ops/s`
    `malloc/free` `25.6M ops/s`
    locked pool `7.01M ops/s`
    `pmr::synchronized_pool_resource` `882.8k ops/s`
- Observation: general-purpose allocation remains much faster than the synchronized pool-style paths once allocation and free happen on different threads, and the PMR synchronized pool is by far the slowest in this benchmark.
- Conclusion: cross-thread ownership transfer is one of the harshest allocator stress patterns because it turns internal synchronization into the dominant cost.
- Key takeaway: allocator strategies that look good in single-owner benchmarks can collapse completely once producer and consumer ownership split across threads.

## 28) `variant` vs Virtual Hierarchy

- Key numbers:
  - `std::variant` dispatch: `553M items/s`
  - virtual hierarchy: `656M items/s`
- Observation: after removing per-object heap-allocation bias and dispatching through stable preallocated objects, the virtual hierarchy is still faster in this mixed-operation benchmark.
- Conclusion: the earlier result was not just an allocation artifact; for this code shape, `std::variant` visitation still loses to virtual dispatch.
- Key takeaway: compare real dispatch patterns directly instead of assuming sum types are always cheaper.

## 29) Container Lookup

- Key numbers:
  - small hot set: `map 123M`, `unordered_map 1.22G`, sorted vector `59.8M items/s`
  - large mixed set with 50% misses: `map 15.5M`, `unordered_map 448M`, sorted vector `17.1M items/s`
- Observation: `unordered_map` wins in both regimes here, but the gap narrows between `map` and sorted-vector lookup in the larger mixed hit/miss case.
- Conclusion: lookup behavior depends materially on keyset size and miss rate, not just the container class name.
- Key takeaway: always test both hot-hit and larger mixed workloads before choosing a lookup container.

## 30) TCP Loopback

- Key numbers:
  - unidirectional stream:
    TCP `1.57M msgs/s`
    Unix stream `1.54M msgs/s`
  - request/response ping-pong:
    TCP `45.5k round trips/s`
    Unix stream `209.6k round trips/s`
- Observation: the transport choice barely matters in the one-way stream test, but matters a great deal in the request/response latency shape.
- Conclusion: throughput and round-trip latency can rank the same transports very differently.
- Key takeaway: network-path benchmarks should include both streaming and ping-pong shapes, not just one direction of traffic.

## 31) Page Fault and `mlock`

- Key numbers:
  - first-touch mapped access: `15.2 GiB/s`
  - prefaulted mapped access: `33.0 GiB/s`
  - `mlock` path: `129.9 GiB/s`, `mlock_ok=1`
- Observation: prefaulting still removes a large part of the first-touch cost, and this rerun successfully obtained locked memory on the current machine.
- Conclusion: page-fault cost is substantial enough to dominate the first pass over a region, and memory locking meaningfully changes the residency story when it actually succeeds.
- Key takeaway: separate first-touch, prefaulted, and locked-memory cases, and always record whether locking actually worked.

## 32) `vector` vs `deque` vs `list`

- Key numbers:
  - `vector`: `12.7G items/s`
  - `deque`: `3.23G items/s`
  - `list`: `934M items/s`
- Observation: contiguous iteration dominates segmented and pointer-linked iteration in this scan-heavy workload.
- Conclusion: sequence-container iteration cost is primarily a locality story, not an API story.
- Key takeaway: for scan-heavy hot paths, `vector` is the default baseline and other sequence containers need a concrete reason to justify their overhead.

## 33) Allocator Variants

- Key numbers:
  - refined rerun with additional PMR pool path:
    `new/delete` `33.3M ops/s`
    `malloc/free` `44.3M ops/s`
    `pmr::monotonic_buffer_resource` `133.8M ops/s`
    `pmr::unsynchronized_pool_resource` `14.7k ops/s`
    arena pool `391.8M ops/s`
- Observation: in this fixed-size single-owner benchmark, the simple arena pool is the clear winner, while the unsynchronized PMR pool resource performs extremely poorly in the current setup.
- Conclusion: allocator abstractions with different recycling policies can land in completely different performance regimes even within the same PMR family.
- Key takeaway: allocator benchmarking has to be specific about object size, recycling policy, and lifetime shape; “PMR” is not one performance point.

## 34) Mixed-Size Allocator Variants

- Key numbers:
  - mixed-size `new/delete`: `36.2M ops/s`
  - mixed-size `malloc/free`: `47.7M ops/s`
  - mixed-size `pmr::unsynchronized_pool_resource`: `133.5M ops/s`
- Observation: the PMR unsynchronized pool is strong in this mixed-size benchmark, which is the opposite of its behavior in the fixed-size allocator benchmark above.
- Conclusion: allocator performance can flip completely when the size distribution and recycling pattern change.
- Key takeaway: allocator selection has to be benchmarked against the actual allocation mix, not just a single synthetic size class.

## 35) `dynamic_cast` vs Tag Dispatch

- Key numbers:
  - enum-tag dispatch: `988.6M items/s`
  - `dynamic_cast` dispatch: `68.0M items/s`
- Observation: RTTI-based type dispatch is roughly an order of magnitude slower than the equivalent tag-based dispatch in this benchmark.
- Conclusion: repeated runtime type checks can dominate hot-path cost when the work per element is small.
- Key takeaway: `dynamic_cast` is fine for cold or structural code paths, but it is a poor default for tight dispatch loops.

## 36) Queue Message Size

- Key numbers:
  - 256-byte payload, unbatched:
    mutex `23.2M msgs/s`, `5.93 GiB/s`
    SPSC `34.5M msgs/s`, `8.83 GiB/s`
  - 256-byte payload, batched-by-8:
    mutex `40.3M msgs/s`, `10.3 GiB/s`
    SPSC `36.5M msgs/s`, `9.35 GiB/s`
- Observation: real batching materially improves the mutex queue in this workload, while the SPSC ring changes only modestly.
- Conclusion: batching can compensate for lock overhead much more than it helps an already lightweight queue path.
- Key takeaway: queue benchmarks should test batching as an explicit algorithmic parameter, not just payload size.

## 37) Aliasing Effects

- Key numbers:
  - potential alias: `10.11G items/s`
  - `restrict`-style no-alias: `10.27G items/s`
  - output aliases input: `3.45G items/s`
- Observation: the no-alias signature is only slightly ahead here, but the true aliasing case where output overlaps input is much slower.
- Conclusion: the biggest aliasing penalty in this benchmark comes from overlapping read/write streams, not from the mere possibility of aliasing in the function signature.
- Key takeaway: aliasing benchmarks should include an actual overlap case; otherwise the result may say more about compiler heuristics than data dependence.

## Final Takeaways

- Cache/locality: stride, pointer chasing, and associativity all show that access pattern sets the upper bound.
- Latency vs throughput: regular sequential access is throughput-friendly; random/conflicting access is latency-dominated.
- Contention/synchronization: false sharing, mutex/atomic, and queue tests all expose shared-write hotspots.
- Data layout: AoS vs SoA should be decided by field utilization and vectorization opportunities.
- Allocation strategy: memory pools need thread-aware design, otherwise synchronization overhead can erase gains.
- Syscall boundary: `mmap` warm-path scans and shared-memory handoff both outperform syscall-heavy alternatives in these runs.
- Language overhead: virtual dispatch and `std::function` both show clear hot-path cost relative to simpler call forms.
- Coordination strategy: spinning is far faster than blocking in the hottest handoff loop, but that result comes with obvious CPU-usage tradeoffs.
- Error signaling: rare thrown exceptions are already expensive enough to materially reshape throughput.
- IPC and transport: shared memory, pipes, Unix-domain sockets, and TCP loopback form a visible cost ladder on the same machine.
- Data structures and allocators: `unordered_map` wins this lookup workload, while globally synchronized pools lose badly in cross-thread ownership transfer.
- Memory residency and container layout: first-touch page cost and non-contiguous container traversal both show how strongly locality and residency shape throughput.
- Allocator and RTTI choice: allocator lifetime model and runtime type-check strategy can both dominate throughput once they enter a hot loop.
- Allocator variance: the same allocator family can look excellent or terrible depending on size mix and recycling policy.
- Queue measurements: synchronization choice, payload size, and topology all materially change which queue design wins.
- Lock behavior: lock rankings shift when the amount of work inside the critical section changes, so contention studies need more than one lock-scope size.
- Measurement discipline: placement and aliasing results both demonstrate that platform behavior must be validated before turning a benchmark into a general claim.
- Platform caveats: affinity and scheduling experiments need explicit validation because API support and enforcement vary by OS.
