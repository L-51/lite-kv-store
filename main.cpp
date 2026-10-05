#include <iostream>
#include <limits>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <thread>

struct Shard{
    std::unordered_map<std::string, std::string> data;
    std::shared_mutex rw_mutex;
};

class KVStore {
private:
    std::vector<Shard> shards;

    /**
     * A routing function, to determine which shard the key belongs to
     * @param key input string
     * @return the shard index
     */
    size_t get_shard_index(const std::string& key) {
        std::hash<std::string> hasher;
        return hasher(key) % shards.size();
    }

public:
    /**
     * Constructor
     */
    KVStore(int num_shards)
        : shards(num_shards){}

    /**
     * Set method
     * @param key
     * @param value
     */
    void set(const std::string& key, const std::string& value) {
        size_t index = get_shard_index(key);

        std::unique_lock lock(shards[index].rw_mutex);

        shards[index].data[key] = value;
    }

    /**
     * Get method
     * @param key
     * @return value associated to the key
     */
    std::string get(const std::string& key) {
        size_t index = get_shard_index(key);

        std::shared_lock lock(shards[index].rw_mutex);

        auto it = shards[index].data.find(key);
        if (it == shards[index].data.end()) {
            std::cerr << "Error: Key '" << key << "' does not exist." << std::endl;
            return "";
        }
        return it->second;
    }
};

void err_msg() {
    std::cerr << "Invalid input\n"
                 "Usage:\n"
                 "\tSET <key> <value>\n"
                 "\tGET <key>\n" << std::endl;
}

void run_stress_test() {
    KVStore db(17);
    std::vector<std::thread> threads;
    int num_threads = 100;

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
    /*
    KVStore kv_store(17);
    std::string order;

    while (std::cin >> order) {
        if (order != "GET" && order != "SET") {
            std::cerr << "Invalid command. Use GET or SET." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::string key, value;

        if (order == "GET") {
            std::cin >> key;
            std::cout << kv_store.get(key) << std::endl;
        }
        else if (order == "SET") {
            std::cin >> key;
            std::cin >> std::ws;
            std::getline(std::cin, value);

            if (key.empty() || value.empty()) {
                err_msg();
            } else {
                kv_store.set(key, value);
                std::cout << key << " set with value " << value << std::endl;
            }
        }
    }
    */

    run_stress_test();
    return 0;
}