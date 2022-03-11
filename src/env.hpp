#pragma once

#include <string>
#include <optional>

namespace env
{
    std::optional<std::string> get_string(const char* name);
    std::optional<bool> get_bool(const char* name);
    // NOTE(Jordan): I currently only need uints, so that's what the type will be
    //               but I should probably make other numeric type getters
    //               for the future.
    std::optional<uint16_t> get_int(const char* name);

    std::string get_string(const char* name, std::string def);
    bool get_bool(const char* name, bool def);
    uint16_t get_int(const char* name, uint16_t def);
}
