#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "buffer.hpp"
#include "model.hpp"
#include "generation.hpp"

#include <charconv>

template<typename T = uint8_t>
auto next(const uint8_t*& ptr) -> decltype(T())
{
    T elem = *((const T*)ptr);
    ptr += sizeof(T);
    return elem;
}

std::string next_string(const uint8_t*& ptr)
{
    auto size = next(ptr);
    std::string str;
    str.resize(size);
    memcpy(str.data(), ptr, size);
    ptr += size;
    return str;
}

TEST_CASE("Buffer writes all integrals", "datagen::buffer")
{
    Buffer buffer;
    buffer.write((char)1);
    buffer.write((short)2);
    buffer.write((int)3);
    buffer.write((long)4);
    buffer.write((unsigned char)5);
    buffer.write((unsigned short)6);
    buffer.write((unsigned int)7);
    buffer.write((unsigned long)8);

    auto ptr = buffer.data();
    REQUIRE(next<char>(ptr) == 1);
    REQUIRE(next<short>(ptr) == 2);
    REQUIRE(next<int>(ptr) == 3);
    REQUIRE(next<long>(ptr) == 4);
    REQUIRE(next<unsigned char>(ptr) == 5);
    REQUIRE(next<unsigned short>(ptr) == 6);
    REQUIRE(next<unsigned int>(ptr) == 7);
    REQUIRE(next<unsigned long>(ptr) == 8);
}

TEST_CASE("Buffer writes all floating points", "datagen::buffer")
{
    Buffer buffer;
    buffer.write((float)2.5f);
    buffer.write((double)9.0);

    auto ptr = buffer.data();
    REQUIRE(next<float>(ptr) == 2.5f);
    REQUIRE(next<double>(ptr) == 9.0);
}

TEST_CASE("Buffer writes prefix-encoded string", "datagen::buffer")
{
    Buffer buffer;
    buffer.write(std::string{"Hello"});

    REQUIRE(buffer.size() == 6);
    REQUIRE(buffer.data()[0] == 5);
    std::string_view view{ (const char*)buffer.data() };
    view.remove_prefix(1);
    REQUIRE(view == "Hello");
}

TEST_CASE("Buffer write vector", "datagen::buffer")
{
    std::vector<int> items { 1, 2, 3, 4, 5 };
    Buffer buffer;
    buffer.write(items);
    auto ptr = buffer.data();
    size_t size = next<size_t>(ptr);
    REQUIRE(size == items.size());
    std::vector<int> read_items {};
    for (size_t i = 0; i < size; ++i)
    {
        int elem = next<int>(ptr);
        REQUIRE(elem == items[i]);
    }
}

TEST_CASE("User model serializes correctly", "datagen::model")
{
    User user;
    user.first_name = "FirstName";
    user.last_name = "LastName";
    user.email = "Email";
    auto buf = user.serialize();
    auto ptr = buf.data();

    auto firstName = next_string(ptr);
    auto lastName = next_string(ptr);
    auto email = next_string(ptr);
    REQUIRE(user.first_name == firstName);
    REQUIRE(user.last_name == lastName);
    REQUIRE(user.email == email);
}

TEST_CASE("Card model serializes correctly", "datagen::model")
{
    Card card;
    card.type = Card::Mastercard;
    card.expiration_month = 5;
    card.expiration_year = 24;
    card.cvv = 215;
    card.pan = "123512351235";
    auto buf = card.serialize();
    auto ptr = buf.data();

    auto type = (Card::CardType)next(ptr);
    auto expiration_month = next(ptr);
    auto expiration_year = next(ptr);
    auto cvv = next<uint>(ptr);
    auto pan = next_string(ptr);

    REQUIRE(card.type == type);
    REQUIRE(card.expiration_month == expiration_month);
    REQUIRE(card.expiration_year == expiration_year);
    REQUIRE(card.cvv == cvv);
    REQUIRE(card.pan == pan);
}

TEST_CASE("Card generation generates valid pan", "datagen::model::card")
{
    Card card = generation::generate_card();

    int s = 0;

    bool is_odd_dgt = true;
    for (ssize_t i = card.pan.size() - 1; i >= 0; --i)
    {
        int k = card.pan[i] - '0';
        s += is_odd_dgt ? k : k != 9 ? (2 * k) % 9 : 9;
        is_odd_dgt = !is_odd_dgt;
    }

    REQUIRE(s % 10 == 0);
}

TEST_CASE("Merchant model serializes correctly", "datagen::model")
{
    Merchant merchant;
    merchant.name = "Test";
    merchant.mcc = 15;
    merchant.category = Merchant::Agricultural;
    auto buf = merchant.serialize();
    auto ptr = buf.data();

    auto name = next_string(ptr);
    auto mcc = next<uint>(ptr);
    auto category = (Merchant::MerchantCategory)next(ptr);

    REQUIRE(merchant.name == name);
    REQUIRE(merchant.mcc == mcc);
    REQUIRE(merchant.category == category);
}

TEST_CASE("Merchant generations provides valid category", "datagen::model::merchant")
{
    Merchant merchant = generation::generate_merchant("3305");

    REQUIRE(merchant.mcc == 3305);
    REQUIRE(merchant.category == Merchant::CarRental);
}
