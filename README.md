# Lite KV-Store (Sharded In-Memory Key-Value Database)

A lightweight, extremely fast, and highly concurrent in-memory Key-Value store written in modern C++.

## Features
- **Sharded Architecture**: Data is partitioned across an array of independent shards using a deterministic hash routing function, drastically reducing lock contention.
- **Highly Concurrent (Thread-Safe)**: Implements a Read-Write lock pattern (`std::shared_mutex`) per shard. Multiple threads can read simultaneously from the same shard, and multiple threads can write simultaneously to different shards.
- **Hardware-Aware Optimization**: Atomic metric counters are aligned to the CPU's `std::hardware_destructive_interference_size` to explicitly eliminate cache line false sharing across cores.
- **Zero-Overhead Observability**: Exposes database operation metrics and latencies in standard Prometheus format, utilizing lock-free `std::atomic` operations with `std::memory_order_relaxed`.
- **Concurrent Benchmarking Suite**: Includes an integrated multithreaded stress-testing matrix that uses atomic spinlocks (`std::memory_order_acquire` / `release`) to guarantee absolute simultaneous thread execution for accurate latency measurements.
- **Deadlock Prevention**: Strictly uses RAII principles (`std::unique_lock` and `std::shared_lock`) to guarantee safe resource acquisition and release.
- **O(1) Time Complexity**: Core storage relies on optimized `std::unordered_map` instances inside each shard for rapid lookups and insertions.
- **CLI Interface**: Includes a lightweight Read-Eval-Print Loop (REPL) for immediate database interaction.
## Benchmarks
Tested locally with 17 shards and 5,000,000 operations per thread:
- **Read-Heavy Workload (100% Reads, 8 Threads):**
  - `std::shared_mutex`: **19.6 Million ops/sec** (50% faster than standard mutex)
- **Mixed Workload (50% Reads / 50% Writes, 8 Threads):**
  - `std::mutex`: **13.3 Million ops/sec** (Outperforms shared_mutex due to reader-writer state switching overhead).
## Motivation
This project was built to demonstrate a deep understanding of memory management, data structures, and multithreading in C++. By transitioning from a monolithic lock to a sharded architecture, this project reflects production-grade concurrency principles essential for high-performance Systems Engineering and backend infrastructure.
