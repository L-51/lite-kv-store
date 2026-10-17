#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "../include/kvstore.h"


namespace { // "Private"
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
        int key_count,
        const std::string& implementation,
        int repetition,
        std::ostream& csv)
    {
        for (int i = 0; i < key_count; ++i) db.set("key_" + std::to_string(i), "initial");

        std::atomic<int> ready{0};
        std::atomic<bool> start{false};

        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::mt19937 rng(static_cast<std::mt19937::result_type>(t));
                std::uniform_int_distribution<int> key_dist(0, key_count - 1);
                std::uniform_int_distribution<int> pct_dist(0, 99);

                ready.fetch_add(1, std::memory_order_release);

                while (!start.load(std::memory_order_acquire)) std::this_thread::yield();

                for (int i = 0; i < operations_per_thread; ++i) {
                    const std::string key = "key_" + std::to_string(key_dist(rng));

                    if (pct_dist(rng) < read_percent) {
                        const auto value = db.get(key);
                        (void)value;
                    } else db.set(key, "update");
                }
            });
        }

        while (ready.load(std::memory_order_acquire) != num_threads) std::this_thread::yield();

        const auto begin = std::chrono::steady_clock::now();
        start.store(true, std::memory_order_release);

        for (auto& thread : threads) thread.join();

        const auto end = std::chrono::steady_clock::now();

        const double seconds = std::chrono::duration<double>(end - begin).count();

        const int total_ops = num_threads * operations_per_thread;

        const double ops_per_sec = total_ops / seconds;

        csv << implementation << ','
            << num_threads << ','
            << read_percent << ','
            << repetition << ','
            << total_ops << ','
            << std::fixed << std::setprecision(9) << seconds << ','
            << std::setprecision(2) << ops_per_sec
            << '\n';
    }

} // namespace

int main() {
#ifdef USE_SHARED_MUTEX
    constexpr const char* implementation = "shared_mutex";
#else
    constexpr const char* implementation = "mutex";
#endif

    constexpr int NUM_SHARDS = 17;
    constexpr int OPERATIONS_PER_THREAD = 5'000'000;
    constexpr int KEY_COUNT = 10'000;
    constexpr int REPETITIONS = 5;

    KVStore db(NUM_SHARDS);

    std::ofstream csv("benchmark_results.csv");

    if (!csv) {
        std::cerr << "Could not open benchmark_results.csv\n";
        return 1;
    }

    csv << "implementation,threads,read_percent,repetition,"
           "operations,seconds,ops_per_sec\n";

    for (int repetition = 1; repetition <= REPETITIONS; ++repetition) {
        for (int threads : {1, 2, 4, 8}) {
            for (int read_percent : {100, 90, 50, 0}) {
                std::cerr
                    << "Running " << implementation
                    << ", repetition " << repetition
                    << ", threads " << threads
                    << ", reads " << read_percent << "%\n";

                benchmark(
                    db,
                    threads,
                    OPERATIONS_PER_THREAD,
                    read_percent,
                    KEY_COUNT,
                    implementation,
                    repetition,
                    csv);
            }
        }
    }

    std::cout << "Results saved to benchmark_results.csv\n";

    return 0;
}