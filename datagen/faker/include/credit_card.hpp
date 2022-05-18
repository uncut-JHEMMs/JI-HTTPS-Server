//
// Created by Shad Shadster on 4/21/2022.
//

#ifndef FAKER_CPP_CREDIT_CARD_HPP
#define FAKER_CPP_CREDIT_CARD_HPP

#include "helpers.hpp"
#include <string>
#include <tuple>
#include <array>
#include <optional>
#include <algorithm>
#include <sstream>

namespace faker::credit_card
{
    enum Type
    {
        AmericanExpress = 0,
        Visa = 1,
        MasterCard = 2,
        Random = 3
    };

    using tuple_type = std::tuple<char, std::optional<std::string_view>, std::array<unsigned int, 2>, unsigned int>;
    constexpr auto make_tuple_type = std::make_tuple<char, std::optional<std::string_view>, std::array<unsigned int, 2>, unsigned  int>;

    constexpr std::array<tuple_type, Type::Random> PREFIX_LENGTHS {
            make_tuple_type('3', "47", {13, 0}, 1),
            make_tuple_type('4', std::nullopt, {12, 15}, 2),
            make_tuple_type('5', "12345", {14, 0}, 1)
    };

    constexpr std::array<unsigned int, Type::Random> SECURITY_CODE_LENGTHS {
        4, 3, 3
    };

    inline static std::string card_number(Type type = Type::Random)
    {
        auto [prefix, opt_prefix, lens, lens_size] =
                type == Type::Random ?
                helpers::array_element(PREFIX_LENGTHS.data(), PREFIX_LENGTHS.size()) :
                PREFIX_LENGTHS[(size_t)type];
        auto len = lens[helpers::number_in_range(0U, lens_size - 1)];

        std::string result{prefix};
        if (opt_prefix.has_value())
            result += helpers::array_element(opt_prefix->data(), opt_prefix->size());

        auto new_len = len - 1 - result.size();

        for (int i = 0; i < new_len; ++i)
            result.append(std::to_string(helpers::number_in_range(0, 9)));

        unsigned int checksum = 0;
        for (int i = result.size() - 1; i >= 0; --i)
            checksum += ((result[i] - '0') * (((i + 1) % 2) + 1)) % 9;
        checksum *= 9;
        checksum %= 10;

        result.append(std::to_string(checksum));
        return result;
    }

    inline static std::string expiration_date()
    {
        std::string result;
        auto month = helpers::number_in_range(1, 12);
        if (month < 10)
            result += '0';
        result.append(std::to_string(month));
        result += '/';
        result.append(std::to_string(helpers::number_in_range(24, 28)));
        return result;
    }

    inline static std::string security_code(Type type = Type::Random)
    {
        auto len =
                type == Type::Random ?
                helpers::array_element(SECURITY_CODE_LENGTHS.data(), SECURITY_CODE_LENGTHS.size()) :
                SECURITY_CODE_LENGTHS[(size_t)type];

        std::string result;
        for (int i = 0; i < len; ++i)
            result.append(std::to_string(helpers::number_in_range(0, 9)));

        return result;
    }
}

#endif //FAKER_CPP_CREDIT_CARD_HPP
