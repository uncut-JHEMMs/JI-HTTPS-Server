#include "datagen.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <charconv>
#include <utility>

#include <lmdb++.h>
#include <uuid/uuid.h>

#include "generation.hpp"

template<typename T, typename CharType, typename Traits>
auto next_as(std::basic_istream<CharType, Traits>& input) -> decltype(T())
{
    std::basic_string<CharType, Traits> line;
    std::getline(input, line);

    if constexpr(std::is_same_v<T, std::basic_string<CharType, Traits>>)
    {
        return line;
    }
    else
    {
        T ret;
        std::from_chars(line.c_str(), line.c_str() + line.size(), ret);
        return ret;
    }
}

template<typename T, typename CharType, typename Traits>
auto next_as(std::basic_istream<CharType, Traits>& input, CharType delim) -> decltype(T())
{
    std::basic_string<CharType, Traits> line;
    std::getline(input, line, delim);

    if constexpr(std::is_same_v<T, std::basic_string<CharType, Traits>>)
    {
        return line;
    }
    else
    {
        T ret;
        std::from_chars(line.c_str(), line.c_str() + line.size(), ret);
        return ret;
    }
}

template<typename CharType, typename Traits>
std::string next_quoted(std::basic_istream<CharType, Traits>& input, CharType delim)
{
    std::basic_string<CharType, Traits> result;
    do
    {
        std::basic_string<CharType, Traits> line;
        std::getline(input, line, delim);
        result += line;
    }
    while (!result.empty() && result[result.size() - 1] != '"');
    return result;
}

void datagen::process(const fs::path& data_path, const fs::path& db_dir)
{
    /**
     * LMDB expects the directory you pass into it to already exist, it won't create
     * it for you. So that's what I'm doing here, making the directory if it doesn't
     * already exist.
     */
    if (!fs::is_directory(db_dir))
        fs::create_directories(db_dir);

    std::ifstream file;
    auto env = lmdb::env::create();
    mdb_env_set_maxdbs(env, 5);
    mdb_env_set_mapsize(env, 1024u * 1024u * 1024u * 10u);
    env.open(db_dir.c_str(), MDB_WRITEMAP);

    auto start = std::chrono::system_clock::now();
    file.open(data_path / "users.ssv");
    {
        auto transaction = lmdb::txn::begin(env);
        MDB_dbi users_dbi = lmdb::dbi::open(transaction, "users", MDB_CREATE);
        std::string user_id_str;
        while (file >> user_id_str)
        {
            uint16_t user_id;
            std::from_chars(user_id_str.c_str(), user_id_str.c_str() + user_id_str.size(), user_id);
            auto user = generation::generate_user();
            auto serialized = user.serialize();

            MDB_val key{ sizeof(uint16_t), &user_id };
            MDB_val val = serialized;
            mdb_put(transaction, users_dbi, &key, &val, 0);
        }
        transaction.commit();
    }
    file.close();

    file.open(data_path / "cards.ssv");
    {
        auto transaction = lmdb::txn::begin(env);
        MDB_dbi cards_dbi = lmdb::dbi::open(transaction, "cards", MDB_CREATE);
        std::string card_data;
        while (file >> card_data)
        {
            std::istringstream in{card_data};
            uint16_t user_id = next_as<uint16_t>(in, ',');
            uint8_t card_id = next_as<uint8_t>(in, ',');

            auto card = generation::generate_card();
            auto serialized = card.serialize();

            Buffer buff(sizeof(uint16_t) + sizeof(uint8_t));
            buff.write(user_id);
            buff.write(card_id);

            MDB_val key = buff;
            MDB_val val = serialized;
            mdb_put(transaction, cards_dbi, &key, &val, 0);
        }
        transaction.commit();
    }
    file.close();

    file.open(data_path / "merchants.ssv");
    {
        auto transaction = lmdb::txn::begin(env);
        MDB_dbi merchants_dbi = lmdb::dbi::open(transaction, "merchants", MDB_CREATE);
        std::string merchant_data;
        while (file >> merchant_data)
        {
            std::istringstream in{merchant_data};
            int64_t merchant_id = next_as<int64_t>(in, ',');
            std::string mcc = next_as<std::string>(in, ',');

            auto merchant = generation::generate_merchant(mcc);
            auto serialized = merchant.serialize();

            MDB_val key{sizeof(merchant_id), &merchant_id};
            MDB_val val = serialized;
            mdb_put(transaction, merchants_dbi, &key, &val, 0);
        }
        transaction.commit();
    }
    file.close();

    file.open(data_path / "states.csv");
    {
        auto transaction = lmdb::txn::begin(env);
        MDB_dbi states_dbi = lmdb::dbi::open(transaction, "states", MDB_CREATE);
        std::string line;
        while (std::getline(file, line))
        {
            std::string abbreviation;
            std::string name;
            std::string capital;
            std::string zip_str;

            typedef struct Range
            {
                uint start;
                uint end;

                Buffer serialize() const
                {
                    Buffer buf{sizeof(uint) * 2};
                    buf.write(start);
                    buf.write(end);
                    return buf;
                }
            } Range;

            std::istringstream iss{line};
            std::getline(iss, abbreviation, ',');
            std::getline(iss, name, ',');
            std::getline(iss, capital, ',');
            std::getline(iss, zip_str, ',');

            zip_str.erase(0, 1);
            if (zip_str[zip_str.size()-1] == '\r')
                zip_str.erase(zip_str.size()-2, 2);
            else
                zip_str.erase(zip_str.size()-1, 1);

            iss = std::istringstream{zip_str};

            std::string range;
            std::vector<Range> ranges;
            while (iss >> range)
            {
                auto start = range.substr(0, range.find('-'));
                auto end = range.substr(range.find('-') + 1);

                uint start_num, end_num;
                std::replace(start.begin(), start.end(), 'n', '0');
                std::replace(end.begin(), end.end(), 'n', '9');
                std::from_chars(start.data(), start.data() + start.size(), start_num);
                std::from_chars(end.data(), end.data() + end.size(), end_num);
                ranges.emplace_back(Range{start_num, end_num});
            }

            Buffer buffer;
            buffer.write(name);
            buffer.write(capital);
            buffer.write(ranges);

            MDB_val key{abbreviation.size(), abbreviation.data()};
            MDB_val val = buffer;
            mdb_put(transaction, states_dbi, &key, &val, 0);
        }
        transaction.commit();
    }
    file.close();

    file.open(data_path / "transactions.csv");
    {
        auto transaction = lmdb::txn::begin(env);
        MDB_dbi offsets_dbi = lmdb::dbi::open(transaction, "user_offsets", MDB_CREATE);

        std::string line;
        uint16_t cur_id = std::numeric_limits<uint16_t>::max();
        while (std::getline(file, line))
        {
            std::istringstream iss{line};
            uint16_t uid = next_as<uint16_t>(iss, ',');

            if (cur_id == uid)
                continue;

            auto read = line.size() + !file.eof();
            uint64_t position = file.gcount() - read;

            MDB_val key{sizeof(uid), &uid};
            MDB_val val{sizeof(position), &position};
            mdb_put(transaction, offsets_dbi, &key, &val, 0);
            cur_id = uid;
        }
        transaction.commit();
    }
    file.close();

    auto end = std::chrono::system_clock::now();

    using ToSeconds = std::chrono::duration<double>;
    auto timeTook = ToSeconds(end - start).count();

    std::cout << "Finished processing in " << timeTook << " seconds\n";
}
