#include "generation.hpp"

#include <faker.hpp>

#include <random>
#include <charconv>
#include <cassert>

std::vector<std::string_view> split_string(const std::string_view& s, const std::string_view& delimiter)
{
    std::vector<std::string_view> tokens;
    size_t last = 0;
    size_t next;
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
    std::string firstName{faker::name::first_name()};
    std::string lastName{faker::name::last_name()};
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
    static std::uniform_int_distribution<> dist(0, faker::credit_card::Random-1);

    auto type = (faker::credit_card::Type)dist(gen);
    auto expiration = faker::credit_card::expiration_date();
    auto elems = split_string(expiration, "/");

    auto expiration_month = parse<uint8_t>(elems[0]);
    auto expiration_year = parse<uint8_t>(elems[1]);
    auto cvv_str = faker::credit_card::security_code(type);
    auto cvv = parse<uint>(cvv_str);
    auto pan = faker::credit_card::card_number(type);

    return {
        (Card::CardType)type,
        expiration_month,
        expiration_year,
        cvv,
        std::move(pan)
    };
}

Merchant generation::generate_merchant(const std::string_view& mcc_str)
{
    auto name = faker::company::name();
    Merchant::MerchantCategory category;

    uint mcc = parse<uint>(mcc_str);

    auto in_range = [](const auto& val, const auto& min, const auto& max) {
        return val >= min && val < max;
    };

    if (in_range(mcc, 0, 1500)) category = Merchant::Agricultural;
    else if (in_range(mcc, 1500, 3000)) category = Merchant::Contracted;
    else if (in_range(mcc, 3000, 3300)) category = Merchant::TravelAndEntertainment;
    else if (in_range(mcc, 3300, 3500)) category = Merchant::CarRental;
    else if (in_range(mcc, 3500, 4000)) category = Merchant::Lodging;
    else if (in_range(mcc, 4000, 4800)) category = Merchant::Transportation;
    else if (in_range(mcc, 4800, 5000)) category = Merchant::Utility;
    else if (in_range(mcc, 5000, 5600)) category = Merchant::RetailOutlet;
    else if (in_range(mcc, 5600, 5700)) category = Merchant::ClothingStore;
    else if (in_range(mcc, 5700, 7300)) category = Merchant::MiscStore;
    else if (in_range(mcc, 7300, 8000)) category = Merchant::Business;
    else if (in_range(mcc, 8000, 9000)) category = Merchant::ProfessionalOrMembership;
    else category = Merchant::Government;

    return {
        std::move(name),
        mcc,
        category
    };
}
