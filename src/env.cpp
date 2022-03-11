#include "env.hpp"

#include <cstdlib>
#include <algorithm>

bool is_integer(const std::string& s);

namespace env
{
    std::optional<std::string> get_string(const char* name)
    {
        if (const char* x = std::getenv(name))
        {
            std::string res = x;
            if (res.empty())
                return {};
            return res;
        }

        return {};
    }

    std::string get_string(const char* name, std::string def)
    {
        return get_string(name).value_or(def);
    }

    std::optional<bool> get_bool(const char* name)
    {
        if (auto optstr = get_string(name))
        {
            auto str = *optstr;
            if (is_integer(str))
            {
                return !!std::stoi(str);
            }

            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            if (str == "true" || str == "on" || str == "yes" || str == "y")
                return true;
            else
                return false;
        }

        return {};
    }

    bool get_bool(const char* name, bool def)
    {
        return get_bool(name).value_or(def);
    }

    std::optional<uint16_t> get_int(const char* name)
    {
        if (auto optstr = get_string(name); optstr && is_integer(*optstr))
        {
            return (uint16_t)std::stoul(*optstr);
        }

        return {};
    }

    uint16_t get_int(const char* name, uint16_t def)
    {
        return get_int(name).value_or(def);
    }
}

bool is_integer(const std::string& s)
{
    if (s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+')))
        return false;

    char* p;
    strtol(s.c_str(), &p, 10);

    return (*p == 0);
}
