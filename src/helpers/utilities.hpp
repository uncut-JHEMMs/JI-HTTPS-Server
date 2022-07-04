#pragma once

#include <string_view>
#include <cstdlib>
#include <cctype>
#include <charconv>
#include <vector>
#include <memory>
#include <cassert>

#include <httpserver.hpp>

namespace util
{
    template<class N> struct is_vector { static constexpr bool value = false; };
    template<class N, class A> struct is_vector<std::vector<N, A>> { static constexpr bool value = true; };
    template<class T> inline constexpr bool is_vector_v = is_vector<T>::value;

    template<class N> struct vector_type { using type = void; using allocator = void; };
    template<class N, class A> struct vector_type<std::vector<N, A>> { using type = N; using allocator = A; };
    template<class T> using vector_type_t = typename vector_type<T>::type;
    template<class T> using vector_type_a = typename vector_type<T>::allocator;

    template<class N> struct promote { using type = N; };
    template<> struct promote<std::string_view> { using type = std::string; };
    template<class T> using promote_t = typename promote<T>::type;

    inline void __assert_condition_failure() {}
    #define CONSTEXPR_ASSERT(...) \
        assert(__VA_ARGS__);      \
        if constexpr(!(__VA_ARGS__)) { __assert_condition_failure(); }

    static inline bool is_integer(const std::string_view& str)
    {
        if (str.empty() || ((!isdigit(str[0])) && (str[0] != '-') && (str[0] != '+'))) return false;

        char* p;
        strtol(str.data(), &p, 10);

        return (*p == 0);
    }

    std::string base64_encode(const uint8_t* buffer, size_t length);
    std::string read_file(const std::string_view& filename);
    std::shared_ptr<httpserver::string_response> make_xml_error(const std::string_view& msg, int code);
    inline std::string to_lower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    template<typename T>
    struct parse_result
    {
        std::errc ec;
        T value;
    };

    template<typename T>
    auto parse(const std::string_view& str) -> parse_result<T>
    {
        CONSTEXPR_ASSERT(std::is_arithmetic_v<T>);
        if constexpr(std::is_same_v<bool, T>)
        {
            parse_result<T> ret;
            if (str == "true" || str == "on" || str == "1")
                ret.value = true;
            else if (str == "false" || str == "off" || str == "0")
                ret.value = false;
            else ret.ec = std::errc::invalid_argument;
            return ret;
        }
        else
        {
            parse_result<T> ret;
            auto [_, ec] { std::from_chars(str.data(), str.data() + str.size(), ret.value) };
            ret.ec = ec;
            return ret;
        }
    }

    template<typename T1, typename Func>
    auto vector_map(const std::vector<T1>& vec, Func func) -> std::vector<decltype(func(vec[0]))>
    {
        std::vector<decltype(func(vec[0]))> new_vec;
        new_vec.reserve(vec.size());
        for (const auto& v : vec)
            new_vec.emplace_back(func(v));
        return new_vec;
    }
}