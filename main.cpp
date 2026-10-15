#include <iostream>
#include <limits>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <thread>
#include <fstream>
#include <chrono>
#include <atomic>

struct Shard {
    std::unordered_map<std::string, std::string> data;
    std::shared_mutex rw_mutex;
};

// Metrics with prometheus format
// hardware_destructive_interference_size forcing a cache line for each metric
struct StoreMetrics {
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> set_ops_total{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> get_ops_total{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> get_misses_total{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> total_set_latency_ns{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> total_get_latency_ns{0};
};

class KVStore {
private:
    std::vector<Shard> shards;
    StoreMetrics metrics;

    size_t get_shard_index(const std::string& key) {
        std::hash<std::string> hasher;
        return hasher(key) % shards.size();
    }

public:
    KVStore(int num_shards) : shards(num_shards){}

    /**
     * Set method
     * @param key
     * @param value
     */
    void set(const std::string& key, const std::string& value) {
        auto start = std::chrono::high_resolution_clock::now();

        size_t index = get_shard_index(key);
        std::unique_lock lock(shards[index].rw_mutex);
        shards[index].data[key] = value;
        lock.unlock();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        metrics.set_ops_total.fetch_add(1, std::memory_order_relaxed);
        metrics.total_set_latency_ns.fetch_add(duration, std::memory_order_relaxed);
    }

    /**
     * Get method
     * @param key
     * @return data associate to key
     */
    std::string get(const std::string& key) {
        auto start = std::chrono::high_resolution_clock::now();

        size_t index = get_shard_index(key);
        std::shared_lock lock(shards[index].rw_mutex);

        auto it = shards[index].data.find(key);
        metrics.get_ops_total.fetch_add(1, std::memory_order_relaxed);

        if (it == shards[index].data.end()) {
            metrics.get_misses_total.fetch_add(1, std::memory_order_relaxed);
            return "";
        }

        std::string val = it->second;
        lock.unlock();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        metrics.total_get_latency_ns.fetch_add(duration, std::memory_order_relaxed);

        return val;
    }

    /**
     * Print the metrics in prometheus format
     */
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

    /**
     * Save the data into a local file
     * @param filename
     */
    void save_to_disk(const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: Could not open " << filename << " for writing." << std::endl;
            return;
        }
        for (size_t i = 0; i < shards.size(); ++i) {
            std::shared_lock lock(shards[i].rw_mutex);
            for (const auto& pair : shards[i].data)
                out << pair.first << "=" << pair.second << "\n";
        }
        out.close();
        std::cout << "Database successfully saved to " << filename << std::endl;
    }

    /**
     * Loading data from a local file
     * @param filename
     */
    void load_from_disk(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) {
            std::cerr << "Error: Could not open " << filename << " for reading." << std::endl;
            return;
        }

        std::string line;
        while (std::getline(in, line)) {
            size_t delimiter_pos = line.find('=');
            if (delimiter_pos != std::string::npos) {
                std::string key = line.substr(0, delimiter_pos);
                std::string value = line.substr(delimiter_pos + 1);
                set(key, value);
            }
        }
        in.close();
        std::cout << "Database loaded from " << filename << std::endl;
    }
};

void err_msg() {
    std::cerr << "Invalid input\n"
                 "Usage:\n"
                 "\tSET <key> <value>\n"
                 "\tGET <key>\n"
                 "\tSAVE <filename>\n"
                 "\tLOAD <filename>\n"
                 "\tMETRICS\n" << std::endl;
}

/**
 * Stressing function, to test the database db
 * @param db
 * @param num_threads
 */
void run_stress_test(KVStore& db, int num_threads) {
    std::vector<std::thread> threads;

    std::cout << "Starting stress test with " << num_threads << " threads..." << std::endl;

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&db, i]() {
            std::string my_key = "key_" + std::to_string(i);
            std::string my_value = "value_" + std::to_string(i);

            db.set(my_key, my_value);
            db.get(my_key);
        }));
    }

    for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }
    std::cout << "Stress test completed successfully! No deadlocks!" << std::endl;
}

int main() {
    const int NUM_THREADS = 1000;
    KVStore kv_store(17);
    std::string order;

    std::cout << "KV-Store Ready. Type a command (SET, GET, SAVE, LOAD, METRICS, STRESS):" << std::endl;

    while (std::cin >> order) {
        if (order != "GET" && order != "SET" && order != "SAVE" && order != "LOAD" && order != "METRICS" && order != "STRESS") {
            std::cerr << "Invalid command." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::string key, value, filename;

        if (order == "GET") {
            std::cin >> key;
            std::string res = kv_store.get(key);
            if (!res.empty()) {
                std::cout << res << std::endl;
            } else {
                std::cout << "(Key not found)" << std::endl;
            }
        }
        else if (order == "SET") {
            std::cin >> key;
            std::cin >> std::ws;
            std::getline(std::cin, value);

            if (key.empty() || value.empty()) err_msg();
            else {
                kv_store.set(key, value);
                std::cout << key << " set with value " << value << std::endl;
            }
        }
        else if (order == "SAVE") {
            std::cin >> filename;
            kv_store.save_to_disk(filename);
        }
        else if (order == "LOAD") {
            std::cin >> filename;
            kv_store.load_from_disk(filename);
        }
        else if (order == "METRICS") {
            kv_store.print_prometheus_metrics();
        }
        else if (order == "STRESS") {
            run_stress_test(kv_store, NUM_THREADS);
        }
    }

    return 0;
}