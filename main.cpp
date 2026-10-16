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
#include <random>
#include <algorithm>
#include <numeric>

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
        // auto start = std::chrono::high_resolution_clock::now();

        size_t index = get_shard_index(key);
        std::unique_lock lock(shards[index].rw_mutex);
        shards[index].data[key] = value;
        lock.unlock();

        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        //
        // metrics.set_ops_total.fetch_add(1, std::memory_order_relaxed);
        // metrics.total_set_latency_ns.fetch_add(duration, std::memory_order_relaxed);
    }

    /**
     * Get method
     * @param key
     * @return data associate to key
     */
    std::string get(const std::string& key) {
        // auto start = std::chrono::high_resolution_clock::now();

        size_t index = get_shard_index(key);
        std::string val;

        {
            #ifdef USE_SHARED_MUTEX
                    std::shared_lock lock(shards[index].rw_mutex);
            #else
                    std::lock_guard lock(shards[index].rw_mutex);
            #endif

            auto it = shards[index].data.find(key);
            metrics.get_ops_total.fetch_add(1, std::memory_order_relaxed);

            if (it == shards[index].data.end()) {
                metrics.get_misses_total.fetch_add(1, std::memory_order_relaxed);
                return "";
            }

            val = it->second;
        }

        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        // metrics.total_get_latency_ns.fetch_add(duration, std::memory_order_relaxed);

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
            #ifdef USE_SHARED_MUTEX
                        std::shared_lock lock(shards[i].rw_mutex);
            #else
                        std::lock_guard lock(shards[i].rw_mutex);
            #endif
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

/**
 *
 * @param db
 * @param num_threads threads to create
 * @param operations_per_thread
 * @param read_percent percentage of operations that are reads
 * @param key_count number of keys prepared in advance
 */
void benchmark(
    KVStore& db,
    int num_threads,
    int operations_per_thread,
    int read_percent,
    int key_count)
{
    for (int i = 0; i < key_count; ++i) db.set("key_" + std::to_string(i), "initial");

    std::atomic<int> ready{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t] {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> key_dist(0, key_count - 1);
            std::uniform_int_distribution<int> pct_dist(0, 99);

            ++ready;
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();

            for (int i = 0; i < operations_per_thread; ++i) {
                std::string key =
                    "key_" + std::to_string(key_dist(rng));

                if (pct_dist(rng) < read_percent) {
                    auto value = db.get(key);
                    (void)value;
                } else {
                    db.set(key, "updated");
                }
            }
        });
    }

    while (ready.load() != num_threads)
        std::this_thread::yield();

    auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& thread : threads) thread.join();

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - begin).count();

    auto total_ops = static_cast<uint64_t>(num_threads) * operations_per_thread;

    std::cout
        << "Threads: " << num_threads
        << ", Read ratio: " << read_percent << "%"
        << ", Total operations: " << total_ops
        << ", Time: " << seconds << " s"
        << ", Throughput: " << total_ops / seconds
        << " ops/s\n";
}

int main() {
    const int NUM_THREADS = 1000;
    KVStore db(17);
    std::string order;

    const int operations_per_thread = 5'000'000;
    const int key_count = 10000;

    for (int threads : {1, 2, 4, 8}) {
        for (int read_percent : {100, 90, 50, 0}) {
            std::cout << "\n--- New test ---\n";

            benchmark(
                db,
                threads,
                operations_per_thread,
                read_percent,
                key_count
            );
        }
    }


    // std::cout << "KV-Store Ready. Type a command (SET, GET, SAVE, LOAD, METRICS, STRESS):" << std::endl;
    //
    // while (std::cin >> order) {
    //     if (order != "GET" && order != "SET" && order != "SAVE" && order != "LOAD" && order != "METRICS" && order != "STRESS") {
    //         std::cerr << "Invalid command." << std::endl;
    //         std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    //         continue;
    //     }
    //
    //     std::string key, value, filename;
    //
    //     if (order == "GET") {
    //         std::cin >> key;
    //         std::string res = db.get(key);
    //         if (!res.empty()) {
    //             std::cout << res << std::endl;
    //         } else {
    //             std::cout << "(Key not found)" << std::endl;
    //         }
    //     }
    //     else if (order == "SET") {
    //         std::cin >> key;
    //         std::cin >> std::ws;
    //         std::getline(std::cin, value);
    //
    //         if (key.empty() || value.empty()) err_msg();
    //         else {
    //             db.set(key, value);
    //             std::cout << key << " set with value " << value << std::endl;
    //         }
    //     }
    //     else if (order == "SAVE") {
    //         std::cin >> filename;
    //         db.save_to_disk(filename);
    //     }
    //     else if (order == "LOAD") {
    //         std::cin >> filename;
    //         db.load_from_disk(filename);
    //     }
    //     else if (order == "METRICS") {
    //         db.print_prometheus_metrics();
    //     }
    //     else if (order == "STRESS") {
    //         run_stress_test(db, NUM_THREADS);
    //     }
    // }

    return 0;
}