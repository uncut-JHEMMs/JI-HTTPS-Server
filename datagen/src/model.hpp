#pragma once

#include <string>
#include "buffer.hpp"

struct User
{
    std::string first_name;
    std::string last_name;
    std::string email;

    inline Buffer serialize()
    {
        /**
         * Calculating the size of this struct in bytes to be serialized.
         * Strings will be prefix length encoded. So the length will be
         * serialized before the string itself, it replaces the null-termination
         * byte, and makes it easier to read later.
         */
        size_t sizeInBytes = first_name.size() * sizeof(char) + sizeof(uint8_t);
        sizeInBytes += last_name.size() * sizeof(char) + sizeof(uint8_t);
        sizeInBytes += email.size() * sizeof(char) + sizeof(uint8_t);

        Buffer buffer{sizeInBytes};
        buffer.write(first_name);
        buffer.write(last_name);
        buffer.write(email);

        return buffer;
    }
};
