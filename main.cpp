#include <iostream>
#include <limits>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <thread>
#include <fstream>

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

    /**
     * Saving the database in a file with "key=value" format
     * @param filename the filename to save to
     */
    void save_to_disk(const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: Could not open " << filename << " for writing." << std::endl;
            return;
        }
        for (int i = 0; i < shards.size(); ++i) {
            std::shared_lock lock(shards[i].rw_mutex);
            for (const auto& pair : shards[i].data)
                out << pair.first << "=" << pair.second << "\n";
        }
        out.close();
        std::cout << "Database successfully saved to " << filename << std::endl;
    }

    /**
     * Extract the data from a file
     * @param filename the filename to load from
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
                 "\tLOAD <filename>\n" << std::endl;
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
    KVStore kv_store(17);
    std::string order;

    std::cout << "KV-Store Ready. Type a command (SET, GET, SAVE, LOAD):" << std::endl;

    while (std::cin >> order) {
        if (order != "GET" && order != "SET" && order != "SAVE" && order != "LOAD") {
            std::cerr << "Invalid command." << std::endl;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::string key, value, filename;

        if (order == "GET") {
            std::cin >> key;
            std::cout << kv_store.get(key) << std::endl;
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
        }else if (order == "SAVE") {
            std::cin >> filename;
            kv_store.save_to_disk(filename);
        }
        else if (order == "LOAD") {
            std::cin >> filename;
            kv_store.load_from_disk(filename);
        }
    }

    // run_stress_test();
    return 0;
}