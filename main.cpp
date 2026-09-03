#include <iostream>
#include <limits>
#include <unordered_map>
#include <string>

class DB {
private:
    std::unordered_map<std::string, std::string> db;

public:
    DB() {}

    /**
     * Set method
     * @param key
     * @param value
     */
    void set(const std::string& key, const std::string& value) {
        db[key] = value;
    }

    /**
     * Get method
     * @param key
     * @return value associated to the key
     */
    std::string get(const std::string& key) {
        auto it = db.find(key);
        if (it == db.end()) {
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

int main() {
    DB db;
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
            std::cout << db.get(key) << std::endl;
        }
        else if (order == "SET") {
            std::cin >> key;
            std::cin >> std::ws;
            std::getline(std::cin, value);

            if (key.empty() || value.empty()) {
                err_msg();
            } else {
                db.set(key, value);
                std::cout << key << " set with value " << value << std::endl;
            }
        }
    }

    return 0;
}