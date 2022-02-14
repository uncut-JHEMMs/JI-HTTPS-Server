#pragma once

#include <string>
#include <optional>

namespace env
{
    std::optional<std::string> get_string(const char* name);
    std::optional<bool> get_bool(const char* name);
    std::optional<int> get_int(const char* name);

    std::string get_string(const char* name, std::string def);
    bool get_bool(const char* name, bool def);
    int get_int(const char* name, int def);
}
