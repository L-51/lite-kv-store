#include <iostream>
#include <thread>
#include <fstream>
#include <chrono>
#include <random>
#include "../include/kvstore.h"

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
    constexpr int NUM_SHARDS = 17;
    constexpr int STRESS_THREADS = 1000;

    KVStore db(NUM_SHARDS);

    std::cout << "KV-Store Ready. Commands: "
        << "SET, GET, SAVE, LOAD, METRICS, STRESS, EXIT\n";

    std::string order;

    while (std::cin >> order) {
        if (order == "EXIT") break;

        std::string key;
        std::string value;
        std::string filename;

        if (order == "GET") {
            if (!(std::cin >> key)) {
                err_msg();
                break;
            }

            const std::string result = db.get(key);

            if (!result.empty()) std::cout << result << '\n';
            else std::cout << "(Key not found)\n";

        } else if (order == "SET") {
            if (!(std::cin >> key >> std::ws)) {
                err_msg();
                break;
            }

            std::getline(std::cin, value);

            if (key.empty() || value.empty()) err_msg();
            else {
                db.set(key, value);
                std::cout << key << " set with value " << value << '\n';
            }

        } else if (order == "SAVE") {
            if (std::cin >> filename)
                db.save_to_disk(filename);
            else {
                err_msg();
                break;
            }

        } else if (order == "LOAD") {
            if (std::cin >> filename) db.load_from_disk(filename);
            else {
                err_msg();
                break;
            }

#ifdef ENABLE_METRICS
        } else if (order == "METRICS") {
            db.print_prometheus_metrics();
#else
        } else if (order == "METRICS") {
            std::cout << "Metrics are disabled. "
                         "Recompile with ENABLE_METRICS.\n";
#endif
        } else if (order == "STRESS") {
            run_stress_test(db, STRESS_THREADS);
        } else {
            err_msg();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    return 0;
}