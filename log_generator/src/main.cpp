#include <fstream>
#include <iostream>
#include <string_view>

constexpr std::string_view endpoints[13] = {
        "/echo", "/empty", "/work",
        "/user", "/transaction_types",
        "/query/transaction", "/query/model", "/query/state",
        "/query/month", "/query/city", "/query/merchant",
        "/query/insuff_bal_percentage", "/query/recurring_transactions"
};

constexpr int status_codes[31] = {
        102, 200, 201, 202,
        204, 301, 302, 304,
        307, 308, 400, 401,
        403, 404, 405, 406,
        408, 412, 415, 426,
        428, 429, 431, 451,
        500, 501, 502, 503,
        504, 505, 511
};

inline std::string_view random_endpoint() { return endpoints[rand() % 13]; }
inline int random_minutes() { return rand() % 60;}
inline int random_status_code() { return status_codes[rand() % 31]; }
inline int random_bytes() { return rand() % 100000; }

int main()
{
    std::ofstream log_file("log.json");
    log_file << "[";
    time_t start = time(nullptr);
    for (int i = 0; i < 1000000; ++i)
    {
        std::string_view endpoint = random_endpoint();
        int minutes = random_minutes();
        int status_code = random_status_code();
        int bytes = random_bytes();
        start += minutes * 60;
        log_file
            << R"({"uri":")" << endpoint
            << R"(","timestamp":)" << start
            << ",\"status_code\":" << status_code
            << ",\"response_size\":" << bytes
            << R"(,"message":"Loren ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.")"
            << "}";

        if (i != 1000000 - 1)
            log_file << "," << std::endl;
    }
    log_file << "]";
    log_file.close();
    std::cout << "Finished writing to log.json!\n";
    return 0;
}