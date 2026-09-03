#include <iostream>
#include <limits>
#include <unordered_map>

class DB
{
    private:
        std::unordered_map<std::string, std::string> db;
    public:
    DB(){}
        /**
         * Set method
         * @param key
         * @param value
         */
        void set(std::string key, std::string value) {db[key] = value;}
        /**
         * Get method
         * @param key
         * @return Value referenced by key
         */
        std::string get(std::string key) {return db[key];}
};

void err_msg() {
    std::cerr << "Invalid input\n"
                             "For example:\n"
                             "\tSET name John\n"
                             "\tGET name\n" << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}


int main() {
    DB db;

    while (true) {
        std::string order;
        std::cin >> order;
        if (order.length() < 3 || (order != "GET" && order != "SET"))
            std::cerr << "Invalid input -> length <= 3 | order != GET or SET" << std::endl;
        else{
            std::string key, value;
            if (order == "GET") {
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (key.length() == 0) err_msg();
                else std::cout << db.get(key) << std::endl;
            }
            else {
                std::cin >> key >> value;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (key.length() == 0 || value.length() == 0) err_msg();
                else {
                    db.set(key, value);
                    std::cout << key << " set with value " << value << std::endl;
                }
            }
        }

    }

    return 0;
}