#include "generation.hpp"

#include "faker.hpp"

#include <random>
#include <charconv>
#include <cassert>

std::vector<std::string_view> split_string(const std::string_view& s, const std::string_view& delimiter)
{
    std::vector<std::string_view> tokens;
    size_t last = 0;
    size_t next = 0;
    while ((next = s.find(delimiter, last)) != std::string_view::npos)
    {
        tokens.push_back(s.substr(last, next - last));
        last = next + 1;
    }
    tokens.push_back(s.substr(last));
    return tokens;
}

template <typename T>
auto parse(const std::string_view& str) -> decltype(T())
{
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
    T number;
    auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(), number);
    assert(ec == std::errc{});
    return number;
}

User generation::generate_user()
{
    std::string firstName = faker::FirstName();
    std::string lastName = faker::LastName();
    std::string email = firstName + "." + lastName + "@smoothceeplusplus.com";

    return {
        std::move(firstName),
        std::move(lastName),
        std::move(email)
    };
}

Card generation::generate_card()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, 4);

    auto type = (Card::CardType)dist(gen);
    auto expiration = faker::credit_card::ExpirationDate();
    auto elems = split_string(expiration, "/");

    auto expiration_month = parse<uint8_t>(elems[0]);
    auto expiration_year = parse<uint8_t>(elems[1]);
    auto cvv_str = faker::credit_card::SecurityCode(type);
    auto cvv = parse<uint>(cvv_str);
    auto pan = faker::credit_card::CardNumber(type);

    return {
        type,
        expiration_month,
        expiration_year,
        cvv,
        std::move(pan)
    };
}
