#!/usr/bin/env python3
"""Run selected benchmarks and generate README figures."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import matplotlib.pyplot as plt


OUT_DIR = Path("assets/plots")


def run(cmd: list[str]) -> str:
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return proc.stdout + proc.stderr


def parse_rate(token: str) -> float:
    token = token.strip()
    m = re.match(r"([0-9]+(?:\.[0-9]+)?)([KMG]?)", token)
    if not m:
        raise ValueError(f"Cannot parse rate token: {token}")
    value = float(m.group(1))
    scale = m.group(2)
    mult = {"": 1.0, "K": 1e3, "M": 1e6, "G": 1e9}[scale]
    return value * mult


def extract_counter(output: str, bench_prefix: str, key: str) -> dict[str, float]:
    out: dict[str, float] = {}
    for line in output.splitlines():
        if bench_prefix not in line or f"{key}=" not in line:
            continue
        bench = line.split()[0]
        m = re.search(rf"{re.escape(key)}=([0-9]+(?:\.[0-9]+)?[KMG]?)", line)
        if not m:
            continue
        out[bench] = parse_rate(m.group(1))
    return out


def extract_cache_sizes_kib(output: str) -> tuple[float | None, float | None]:
    l1_kib = None
    l2_kib = None
    m1 = re.search(r"L1 Data\s+([0-9]+)\s+KiB", output)
    m2 = re.search(r"L2 Unified\s+([0-9]+)\s+KiB", output)
    if m1:
        l1_kib = float(m1.group(1))
    if m2:
        l2_kib = float(m2.group(1))
    return l1_kib, l2_kib


def ensure_bins() -> None:
    bins = [
        "build/benchmark/bm_cache_levels",
        "build/benchmark/bm_cache_associativity",
        "build/benchmark/bm_queue",
        "build/benchmark/bm_memory_pool",
    ]
    if not all(Path(b).exists() for b in bins):
        run(["cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"])
        run(["cmake", "--build", "build", "-j"])


def save(fig: plt.Figure, name: str) -> None:
    fig.tight_layout()
    fig.savefig(OUT_DIR / name, dpi=180)
    plt.close(fig)


def plot_cache_levels() -> None:
    output = run(
        [
            "./build/benchmark/bm_cache_levels",
            "--benchmark_min_time=0.05s",
            "--benchmark_report_aggregates_only=true",
        ]
    )
    rates = extract_counter(output, "BM_CacheLevels/", "items_per_second")
    l1_kib, l2_kib = extract_cache_sizes_kib(output)
    points = []
    for name, rate in rates.items():
        size = int(name.split("/")[-1])
        points.append((size / 1024.0, rate / 1e6))
    points.sort(key=lambda x: x[0])

    x = [p[0] for p in points]
    y = [p[1] for p in points]

    fig, ax = plt.subplots(figsize=(7.4, 4.2))
    ax.plot(x, y, marker="o", linewidth=1.8, markersize=3.5, color="#1d3557")
    ax.set_title("Cache Levels: Working Set vs Throughput")
    ax.set_xlabel("Working Set (KiB, log2 scale)")
    ax.set_ylabel("Items/s (M)")
    ax.set_xscale("log", base=2)
    ax.grid(alpha=0.28)
    ymax = max(y) if y else 1.0
    if l1_kib is not None:
        ax.axvline(l1_kib, color="#6c757d", linestyle="--", linewidth=1.2)
        ax.text(l1_kib * 1.03, ymax * 0.95, f"L1D {int(l1_kib)} KiB", color="#495057", fontsize=9)
    if l2_kib is not None:
        ax.axvline(l2_kib, color="#6c757d", linestyle="--", linewidth=1.2)
        ax.text(l2_kib * 1.03, ymax * 0.88, f"L2 {int(l2_kib)} KiB", color="#495057", fontsize=9)
    save(fig, "cache_levels_curve.png")


def plot_associativity() -> None:
    output = run(
        [
            "./build/benchmark/bm_cache_associativity",
            "--benchmark_min_time=0.05s",
            "--benchmark_report_aggregates_only=true",
        ]
    )
    friendly = extract_counter(output, "BM_AssocFriendly/", "items_per_second")
    conflict = extract_counter(output, "BM_AssocConflict/", "items_per_second")

    def sort_points(data: dict[str, float]) -> tuple[list[int], list[float]]:
        points = []
        for name, rate in data.items():
            lines = int(name.split("/")[-1])
            points.append((lines, rate / 1e6))
        points.sort(key=lambda x: x[0])
        return [p[0] for p in points], [p[1] for p in points]

    fx, fy = sort_points(friendly)
    cx, cy = sort_points(conflict)

    fig, ax = plt.subplots(figsize=(7.4, 4.2))
    ax.plot(fx, fy, marker="o", linewidth=1.8, markersize=3.5, label="Friendly stride", color="#2a9d8f")
    ax.plot(cx, cy, marker="o", linewidth=1.8, markersize=3.5, label="Conflict stride", color="#e63946")
    ax.set_title("Cache Associativity Effect")
    ax.set_xlabel("Active lines")
    ax.set_ylabel("Items/s (M)")
    ax.grid(alpha=0.28)
    ax.legend()
    save(fig, "associativity_conflict.png")


def plot_queue_memorypool() -> None:
    q_out = run(
        [
            "./build/benchmark/bm_queue",
            "--benchmark_min_time=0.05s",
            "--benchmark_filter=BM_Queue(MutexTransfer|SpscRingTransfer).*",
            "--benchmark_report_aggregates_only=true",
        ]
    )
    p_out = run(
        [
            "./build/benchmark/bm_memory_pool",
            "--benchmark_min_time=0.05s",
            "--benchmark_report_aggregates_only=true",
        ]
    )

    q_rates = extract_counter(q_out, "BM_Queue", "ops_per_sec")
    p_rates = extract_counter(p_out, "BM_", "ops_per_sec")

    # queue lines: x=batch, one line per (impl, backoff)
    queue_series: dict[tuple[str, int], dict[int, float]] = {}
    for name, rate in q_rates.items():
        if "_mean" in name or "_median" in name or "_stddev" in name or "_cv" in name:
            continue
        impl = "Mutex" if "BM_QueueMutexTransfer" in name else "SPSC"
        b_m = re.search(r"batch:(\d+)", name)
        bo_m = re.search(r"backoff:(\d+)", name)
        if not b_m or not bo_m:
            continue
        batch = int(b_m.group(1))
        backoff = int(bo_m.group(1))
        queue_series.setdefault((impl, backoff), {})[batch] = rate / 1e6

    fig, axs = plt.subplots(1, 2, figsize=(11.0, 4.2))

    backoff_label = {0: "yield", 1: "spin", 2: "hybrid"}
    for (impl, backoff), vals in sorted(queue_series.items()):
        xs = sorted(vals.keys())
        ys = [vals[x] for x in xs]
        color = "#2a9d8f" if impl == "SPSC" else "#6d597a"
        style = {0: "-", 1: "--", 2: ":"}[backoff]
        axs[0].plot(xs, ys, marker="o", linestyle=style, color=color, linewidth=1.6, markersize=3.5,
                    label=f"{impl}-{backoff_label[backoff]}")

    axs[0].set_title("Queue Throughput by Config")
    axs[0].set_xlabel("Batch size")
    axs[0].set_ylabel("Ops/s (M)")
    axs[0].set_xscale("log", base=2)
    axs[0].set_xticks([1, 8, 64], ["1", "8", "64"])
    axs[0].grid(alpha=0.28)
    axs[0].legend(fontsize=8, ncol=2)

    pool_order = [
        "BM_NewDeleteMultiThread",
        "BM_LockedPoolMultiThread",
        "BM_ThreadLocalPoolMultiThread",
    ]
    pool_names = ["new/delete", "locked pool", "thread-local pool"]
    pool_vals = [p_rates.get(k, 0.0) / 1e6 for k in pool_order]

    axs[1].bar(pool_names, pool_vals, color=["#457b9d", "#e63946", "#2a9d8f"])
    axs[1].set_title("Memory Pool Throughput")
    axs[1].set_ylabel("Ops/s (M)")
    axs[1].tick_params(axis="x", rotation=14)
    axs[1].grid(axis="y", alpha=0.28)

    save(fig, "queue_memorypool.png")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ensure_bins()
    plot_cache_levels()
    plot_associativity()
    plot_queue_memorypool()
    print(f"Generated plots in: {OUT_DIR}")


if __name__ == "__main__":
    main()
