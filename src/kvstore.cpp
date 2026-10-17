#include "../include/kvstore.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

size_t KVStore::get_shard_index(const std::string& key) const {
    return std::hash<std::string>()(key) % shards.size();
}

KVStore::KVStore(int num_shards)
    : shards(static_cast<size_t>(num_shards > 0 ? num_shards : 1)) {}

void KVStore::set(const std::string& key, const std::string& value) {
#ifdef ENABLE_METRICS
    const auto start = std::chrono::steady_clock::now();
#endif

    const size_t index = get_shard_index(key);

    {
        std::unique_lock lock(shards[index].rw_mutex);
        shards[index].data[key] = value;
    }

#ifdef ENABLE_METRICS
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    metrics.set_ops_total.fetch_add(1, std::memory_order_relaxed);
    metrics.total_set_latency_ns.fetch_add(duration, std::memory_order_relaxed);
#endif
}

std::string KVStore::get(const std::string& key) {
#ifdef ENABLE_METRICS
    const auto start = std::chrono::steady_clock::now();
#endif

    const size_t index = get_shard_index(key);
    std::string value;
    bool found = false;

    {
#ifdef USE_SHARED_MUTEX
        std::shared_lock lock(shards[index].rw_mutex);
#else
        std::lock_guard lock(shards[index].rw_mutex);
#endif

        const auto it = shards[index].data.find(key);
        found = it != shards[index].data.end();

        if (found) value = it->second;
    }

#ifdef ENABLE_METRICS
    const auto end = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    metrics.get_ops_total.fetch_add(1, std::memory_order_relaxed);
    metrics.total_get_latency_ns.fetch_add(duration, std::memory_order_relaxed);

    if (!found) metrics.get_misses_total.fetch_add(1, std::memory_order_relaxed);
#endif

    return value;
}

void KVStore::save_to_disk(const std::string& filename) {
    std::ofstream out(filename);

    if (!out.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing.\n";
        return;
    }

    for (auto& shard : shards) {
#ifdef USE_SHARED_MUTEX
        std::shared_lock lock(shard.rw_mutex);
#else
        std::lock_guard<RWMutex> lock(shard.rw_mutex);
#endif

        for (const auto& [key, value] : shard.data) out << key << "=" << value << '\n';
    }

    if (!out) {
        std::cerr << "Error while writing to " << filename << '\n';
        return;
    }

    out.close();
    std::cout << "Database successfully saved to " << filename << '\n';
}

void KVStore::load_from_disk(const std::string& filename) {
    std::ifstream in(filename);

    if (!in.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for reading.\n";
        return;
    }

    std::string line;

    while (std::getline(in, line)) {
        const size_t delimiter_pos = line.find('=');

        if (delimiter_pos != std::string::npos) {
            const std::string key = line.substr(0, delimiter_pos);
            const std::string value = line.substr(delimiter_pos + 1);
            set(key, value);
        }
    }

    in.close();
    std::cout << "Database loaded from " << filename << '\n';
}

#ifdef ENABLE_METRICS

void print_prometheus_metrics() {
    uint64_t sets = metrics.set_ops_total.load(std::memory_order_relaxed);
    uint64_t gets = metrics.get_ops_total.load(std::memory_order_relaxed);
    uint64_t misses = metrics.get_misses_total.load(std::memory_order_relaxed);
    uint64_t set_lat = metrics.total_set_latency_ns.load(std::memory_order_relaxed);
    uint64_t get_lat = metrics.total_get_latency_ns.load(std::memory_order_relaxed);

    double avg_set_ms = sets ? (double)set_lat / sets / 1e6 : 0.0;
    double avg_get_ms = gets ? (double)get_lat / gets / 1e6 : 0.0;

    std::cout << "\n# HELP kvstore_operations_total Total number of operations shorthand by type.\n"
              << "# TYPE kvstore_operations_total counter\n"
              << "kvstore_operations_total{type=\"set\"} " << sets << "\n"
              << "kvstore_operations_total{type=\"get\"} " << gets << "\n"
              << "kvstore_operations_total{type=\"miss\"} " << misses << "\n"
              << "# HELP kvstore_average_latency_milliseconds Average operation execution time.\n"
              << "# TYPE kvstore_average_latency_milliseconds gauge\n"
              << "kvstore_average_latency_milliseconds{type=\"set\"} " << avg_set_ms << "\n"
              << "kvstore_average_latency_milliseconds{type=\"get\"} " << avg_get_ms << "\n" << std::endl;
}
#endif