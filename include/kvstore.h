#ifndef KVSTORE_H
#define KVSTORE_H

#include <chrono>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USE_SHARED_MUTEX
    using RWMutex = std::shared_mutex;
#else
    using RWMutex = std::mutex;
#endif

struct Shard {
    std::unordered_map<std::string, std::string> data;
    RWMutex rw_mutex;
};

// Metrics with prometheus format
// hardware_destructive_interference_size forcing a cache line for each metric
#ifdef ENABLE_METRICS
struct StoreMetrics {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<uint64_t> set_ops_total{0};

    alignas(std::hardware_destructive_interference_size)
        std::atomic<uint64_t> get_ops_total{0};

    alignas(std::hardware_destructive_interference_size)
        std::atomic<uint64_t> get_misses_total{0};

    alignas(std::hardware_destructive_interference_size)
        std::atomic<uint64_t> total_set_latency_ns{0};

    alignas(std::hardware_destructive_interference_size)
        std::atomic<uint64_t> total_get_latency_ns{0};
};
#endif

class KVStore {
private:
    std::vector<Shard> shards;

#ifdef ENABLE_METRICS
    StoreMetrics metrics;
#endif

    size_t get_shard_index(const std::string& key) const;

public:
    KVStore(int num_shards);

    /**
     * Set method
     * @param key
     * @param value
     */
    void set(const std::string& key, const std::string& value);
    /**
     * Get method
     * @param key
     * @return data associate to key
     */
    std::string get(const std::string& key);

    /**
     * Save the data into a local file
     * @param filename
     */
    void save_to_disk(const std::string& filename);
    /**
     * Loading data from a local file
     * @param filename
     */
    void load_from_disk(const std::string& filename);

#ifdef ENABLE_METRICS
    /**
     * Print the metrics in prometheus format
     */
    void print_prometheus_metrics() const;
#endif
};

#endif // KVSTORE_H