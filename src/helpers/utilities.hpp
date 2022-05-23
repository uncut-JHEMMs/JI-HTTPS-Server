#pragma once

#include <string_view>
#include <cstdlib>
#include <cctype>

namespace util
{
    static inline bool is_integer(const std::string_view& str)
    {
        if (str.empty() || ((!isdigit(str[0])) && (str[0] != '-') && (str[0] != '+'))) return false;

        char* p;
        strtol(str.data(), &p, 10);

        return (*p == 0);
    }

    std::string base64_encode(const uint8_t* buffer, size_t length);
    std::string read_file(const std::string_view& filename);
}