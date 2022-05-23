//
// Created by Shad Shadster on 4/21/2022.
//

#ifndef FAKER_CPP_HELPERS_H
#define FAKER_CPP_HELPERS_H

#include <cstdint>
#include <cstring>
#include <random>
#include <algorithm>
#include <type_traits>

namespace faker::helpers
{
    template<typename T, typename std::enable_if<std::is_floating_point<T>::value, T>::type* = nullptr>
    inline static T number_in_range(T min, T max)
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};
        std::uniform_real_distribution<T> dist{min, max};
        return dist(gen);
    }

    template<typename T, typename std::enable_if<std::numeric_limits<T>::is_integer, T>::type* = nullptr>
    inline static T number_in_range(T min, T max)
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};
        std::uniform_int_distribution<T> dist{min, max};
        return dist(gen);
    }

    template<typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
    inline static auto number() -> decltype(T())
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};
        std::uniform_real_distribution<T> dist{};
        return dist(gen);
    }

    template<typename T, typename std::enable_if<!std::is_floating_point<T>::value && std::is_arithmetic<T>::value>::type* = nullptr>
    inline static auto number() -> decltype(T())
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};
        std::uniform_int_distribution<T> dist{};
        return dist(gen);
    }

    template<typename T>
    inline static const T& array_element(const T* const array, size_t size)
    {
        auto index = number_in_range(size_t(0), size - 1);
        return array[index];
    }

    template<typename T>
    inline static const T& array_element(const std::vector<T>& array)
    {
        auto index = number_in_range(0ULL, array.size() - 1);
        return array.at(index);
    }

    inline static std::string replace_sym_with_number(const std::string& string)
    {
        std::string output;
        for (auto c : string)
        {
            switch (c)
            {
            case 'X':
            case '#':
                output.append(std::to_string(number_in_range(0, 9)));
                break;
            case 'Z':
                output.append(std::to_string(number_in_range(1, 9)));
                break;
            case 'N':
                output.append(std::to_string(number_in_range(2, 9)));
                break;
            default:
                output += c;
            }
        }
        return output;
    }

    inline static void sentence_case(std::string& str)
    {
        auto start = str.begin();
        *start = ::toupper(*start);
    }

    inline static void lowercase(std::string& str)
    {
        auto start = str.begin();
        *start = ::tolower(*start);
    }

    template<typename T>
    inline static void shuffle(T* array, size_t size)
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};

        std::shuffle(std::cbegin(array), std::cend(array + size), gen);
    }

    template<typename T>
    inline static void shuffle(std::vector<T>& array)
    {
        std::random_device rd;
        std::mt19937_64 gen{rd()};

        std::shuffle(array.begin(), array.end(), gen);
    }
}

#endif //FAKER_CPP_HELPERS_H
