#include "resources.hpp"

#include <fstream>
#include <charconv>
#include <optional>
#include <iomanip>

#include "models.hpp"
#include "helpers/xml_builder.hpp"
#include "helpers/utilities.hpp"

namespace resources::analytics
{
    template<typename T>
    using top5 = std::vector<T>;

    using httpserver::string_response;

    const Ref<http_response> get_top5_transactions_by_zip::process(const http_request& req)
    {
        auto& args = req.get_args();
        int count = -1;
        if (args.find("count") != args.end() && util::is_integer(args.at("count")))
        {
            auto count_str = args.at("count");
            std::from_chars(count_str.c_str(), count_str.c_str() + count_str.size(), count);
        }

        using user_amount = std::pair<uint16_t, long>;
        std::map<uint32_t, top5<user_amount>> top5_by_zip;

        std::ifstream transactions("data/transactions.csv");
        std::string line;
        std::getline(transactions, line);
        while (std::getline(transactions, line))
        {
            std::istringstream iss{line};

            auto get_id = [&iss]()
            {
                std::string id_str;
                std::getline(iss, id_str, ',');
                uint16_t id;
                std::from_chars(id_str.c_str(), id_str.c_str() + id_str.size(), id);
                return id;
            };
            auto get_zip = [&iss](bool& online) -> uint32_t
            {
                std::string zip_str;
                std::getline(iss, zip_str, ',');
                if (zip_str.empty())
                {
                    online = true;
                    return 0;
                }
                else
                {
                    online = false;
                }

                zip_str.erase(zip_str.size() - 2);
                uint32_t zip;
                std::from_chars(zip_str.c_str(), zip_str.c_str() + zip_str.size(), zip);
                return zip;
            };
            auto get_amount = [&iss]() -> long
            {
                std::string amount_str;
                std::getline(iss, amount_str, ',');
                amount_str.erase(0, 1);
                bool negative = false;
                if (amount_str[0] == '-')
                {
                    negative = true;
                    amount_str.erase(0, 1);
                }

                std::string cent_str = amount_str;
                auto idx = amount_str.find('.');
                amount_str.erase(idx);
                cent_str.erase(0, idx + 1);

                long dollars, cents;
                std::from_chars(amount_str.c_str(), amount_str.c_str() + amount_str.size(), dollars);
                std::from_chars(cent_str.c_str(), cent_str.c_str() + cent_str.size(), cents);

                return (dollars * 100 + cents) *  (negative ? -1 : 1);
            };
            auto skip = [&iss](){std::string _;std::getline(iss, _, ',');};
            auto next_quoted = [&iss]()
            {
                std::string result;
                do
                {
                    std::string _;
                    std::getline(iss, _, ',');
                    result += _;
                }
                while (!result.empty() && result[result.size() - 1] != '"');
                return result;
            };

            auto user_id = get_id();
            for (int i = 0; i < 5; ++i)
                skip();
            auto amount = get_amount();
            if (amount <= 0)
                continue;
            for (int i = 0; i < 4; ++i)
                skip();
            bool online;
            auto zip = get_zip(online);
            if (online)
                continue;
            skip();
            auto errors = next_quoted();
            if (!errors.empty())
                continue;
            std::string fraud;
            std::getline(iss, fraud, ',');
            if (fraud != "No")
                continue;

            if (!top5_by_zip.count(zip))
                top5_by_zip.insert(std::make_pair(zip, top5<user_amount>{}));

            if (top5_by_zip[zip].size() < 5)
            {
                top5_by_zip[zip].push_back(std::make_pair(user_id, amount));
                std::sort(top5_by_zip[zip].begin(), top5_by_zip[zip].end(), [](const user_amount& pairA, const user_amount& pairB) {
                    return pairA.second > pairB.second;
                });
            }
            else if (top5_by_zip[zip][4].second < amount)
            {
                top5_by_zip[zip].pop_back();
                top5_by_zip[zip].push_back(std::make_pair(user_id, amount));
                std::sort(top5_by_zip[zip].begin(), top5_by_zip[zip].end(), [](const user_amount& pairA, const user_amount& pairB) {
                    return pairA.second > pairB.second;
                });
            }
        }

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_child("Top5Transactions", {{"GroupedBy", "zip"}});

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto dbi = lmdb::dbi::open(rtxn, "users");
        auto cursor = lmdb::cursor::open(rtxn, dbi);

        auto read_string = [](uint8_t*& raw)
        {
            uint8_t size = *raw;
            raw++;
            std::string str;
            str.resize(size);
            memcpy(str.data(), raw, size);
            raw += size;
            return str;
        };

        int step = 0;
        for (const auto& [zip, user_n_amount] : top5_by_zip)
        {
            if (count != -1)
                step++;
            builder
                .add_array("Result", {{"Zip", std::to_string(zip)}}, user_n_amount, [&cursor, &read_string](XmlBuilder& b, const user_amount& pair) {
                    uint16_t id = pair.first;
                    MDB_val key{sizeof(id), &id};
                    MDB_val result;

                    if (mdb_cursor_get(cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                    {
                        auto* raw = (uint8_t*)result.mv_data;
                        std::string first_name = read_string(raw);
                        std::string last_name = read_string(raw);
                        std::string email = read_string(raw);

                        b
                            .add_child("User", {{ "ID", std::to_string(pair.first) }})
                                .add_string("FirstName", first_name)
                                .add_string("LastName", last_name)
                                .add_string("Email", email)
                            .step_up();

                        std::string amount_str = "$";
                        amount_str += std::to_string(pair.second / 100);
                        amount_str += '.';
                        amount_str += std::to_string(pair.second % 100);
                        b.add_string("Amount", amount_str);
                    }
                });
            if (step >= count)
                break;
        }

        cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }

    const Ref<http_response> get_top5_transactions_by_city::process(const http_request& req)
    {
        auto& args = req.get_args();
        int count = -1;
        if (args.find("count") != args.end() && util::is_integer(args.at("count")))
        {
            auto count_str = args.at("count");
            std::from_chars(count_str.c_str(), count_str.c_str() + count_str.size(), count);
        }

        using user_amount = std::pair<uint16_t, long>;
        std::map<std::string, top5<user_amount>> top5_by_city;

        std::ifstream transactions("data/transactions.csv");
        std::string line;
        std::getline(transactions, line);
        while (std::getline(transactions, line))
        {
            std::istringstream iss{line};

            auto get_id = [&iss]()
            {
                std::string id_str;
                std::getline(iss, id_str, ',');
                uint16_t id;
                std::from_chars(id_str.c_str(), id_str.c_str() + id_str.size(), id);
                return id;
            };
            auto get_amount = [&iss]() -> long
            {
                std::string amount_str;
                std::getline(iss, amount_str, ',');
                amount_str.erase(0, 1);
                bool negative = false;
                if (amount_str[0] == '-')
                {
                    negative = true;
                    amount_str.erase(0, 1);
                }

                std::string cent_str = amount_str;
                auto idx = amount_str.find('.');
                amount_str.erase(idx);
                cent_str.erase(0, idx + 1);

                long dollars, cents;
                std::from_chars(amount_str.c_str(), amount_str.c_str() + amount_str.size(), dollars);
                std::from_chars(cent_str.c_str(), cent_str.c_str() + cent_str.size(), cents);

                return (dollars * 100 + cents) *  (negative ? -1 : 1);
            };
            auto skip = [&iss](){std::string _;std::getline(iss, _, ',');};
            auto next_quoted = [&iss]()
            {
                std::string result;
                do
                {
                    std::string _;
                    std::getline(iss, _, ',');
                    result += _;
                }
                while (!result.empty() && result[result.size() - 1] != '"');
                return result;
            };

            auto user_id = get_id();
            for (int i = 0; i < 5; ++i)
                skip();
            auto amount = get_amount();
            if (amount <= 0)
                continue;
            for (int i = 0; i < 2; ++i)
                skip();
            std::string city;
            std::getline(iss, city, ',');
            if (city == " ONLINE")
                continue;
            for (int i = 0; i < 3; ++i)
                skip();
            auto errors = next_quoted();
            if (!errors.empty())
                continue;
            std::string fraud;
            std::getline(iss, fraud, ',');
            if (fraud != "No")
                continue;

            if (!top5_by_city.count(city))
                top5_by_city.insert(std::make_pair(city, top5<user_amount>{}));

            if (top5_by_city[city].size() < 5)
            {
                top5_by_city[city].push_back(std::make_pair(user_id, amount));
                std::sort(top5_by_city[city].begin(), top5_by_city[city].end(), [](const user_amount& pairA, const user_amount& pairB) {
                    return pairA.second > pairB.second;
                });
            }
            else if (top5_by_city[city][4].second < amount)
            {
                top5_by_city[city].pop_back();
                top5_by_city[city].push_back(std::make_pair(user_id, amount));
                std::sort(top5_by_city[city].begin(), top5_by_city[city].end(), [](const user_amount& pairA, const user_amount& pairB) {
                    return pairA.second > pairB.second;
                });
            }
        }

        XmlBuilder builder;
        builder
                .add_signature()
                .add_child("Data")
                .add_child("Top5Transactions", {{"GroupedBy", "city"}});

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto dbi = lmdb::dbi::open(rtxn, "users");
        auto cursor = lmdb::cursor::open(rtxn, dbi);

        auto read_string = [](uint8_t*& raw)
        {
            uint8_t size = *raw;
            raw++;
            std::string str;
            str.resize(size);
            memcpy(str.data(), raw, size);
            raw += size;
            return str;
        };

        int step = 0;
        for (const auto& [city, user_n_amount] : top5_by_city)
        {
            if (count != -1)
                step++;
            builder
                .add_array("Result", {{"City", city}}, user_n_amount, [&cursor, &read_string](XmlBuilder& b, const user_amount& pair) {
                    uint16_t id = pair.first;
                    MDB_val key{sizeof(id), &id};
                    MDB_val result;

                    if (mdb_cursor_get(cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                    {
                        auto* raw = (uint8_t*)result.mv_data;
                        std::string first_name = read_string(raw);
                        std::string last_name = read_string(raw);
                        std::string email = read_string(raw);

                        b
                                .add_child("User", {{ "ID", std::to_string(pair.first) }})
                                .add_string("FirstName", first_name)
                                .add_string("LastName", last_name)
                                .add_string("Email", email)
                                .step_up();

                        std::string amount_str = "$";
                        amount_str += std::to_string(pair.second / 100);
                        amount_str += '.';
                        amount_str += std::to_string(pair.second % 100);
                        b.add_string("Amount", amount_str);
                    }
                });
            if (step >= count)
                break;
        }

        cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }

    template<typename T = std::string>
    auto next_as(std::istream& input, char delim = ',', bool quoted = false) -> decltype(T())
    {
        std::string line;
        if (quoted)
        {
            do
            {
                std::string _;
                std::getline(input, _, delim);
                line += _;
            }
            while (!line.empty() && line[line.size() - 1] != '"');

            if (!line.empty())
            {
                line.erase(0, 1);
                line.erase(line.size() - 1, 1);
            }
        }
        else
        {
            std::getline(input, line, delim);
        }

        if constexpr(std::is_same_v<T, std::string>)
        {
            return line;
        }
        else if constexpr(std::is_same_v<T, bool>)
        {
            return line == "Yes";
        }
        else if constexpr(std::is_same_v<T, models::TransactionType>)
        {
            if (line == "Chip Transaction")
                return models::TransactionType::Chip;
            else if (line == "Online Transaction")
                return models::TransactionType::Online;
            else if (line == "Swipe Transaction")
                return models::TransactionType::Swipe;
            else
                return models::TransactionType::Unknown;
        }
        else if constexpr(std::is_same_v<T, std::optional<uint32_t>>)
        {
            if (line.empty())
                return std::nullopt;
            uint32_t ret;
            std::from_chars(line.c_str(), line.c_str() + line.size(), ret);
            return ret;
        }
        else
        {
            T ret;
            std::from_chars(line.c_str(), line.c_str() + line.size(), ret);
            return ret;
        }
    }

    template<class N>
    struct is_vector { static constexpr bool value = false; };
    template<class N, class A>
    struct is_vector<std::vector<N, A>> { static constexpr bool value = true; };

    template<class T>
    inline constexpr bool is_vector_v = is_vector<T>::value;

    template<class N>
    struct vector_types { using Type = std::nullopt_t; using Allocator = std::nullopt_t; };

    template<class N, class A>
    struct vector_types<std::vector<N, A>> { using Type = N; using Allocator = A; };

    template<typename T = uint8_t>
    auto next_as(const uint8_t*& ptr) -> decltype(T())
    {
        if constexpr(std::is_same_v<T, std::string>)
        {
            auto size = next_as(ptr);
            std::string str;
            str.resize(size);
            memcpy(str.data(), ptr, size);
            ptr += size;
            return str;
        }
        else if constexpr(is_vector_v<T>)
        {
            auto size = next_as<uint64_t>(ptr);
            std::vector<typename vector_types<T>::Type> vector(size);
            memcpy(vector.data(), ptr, sizeof(typename vector_types<T>::Type) * size);
            ptr += size * sizeof(typename vector_types<T>::Type);
            return vector;
        }
        else
        {
            T elem = *((const T*)ptr);
            ptr += sizeof(T);
            return elem;
        }
    }

    models::Transaction parse_transaction(const std::string& line)
    {
        std::istringstream iss{line};
        auto get_amount = [&iss]() -> long {
            std::string amount_str;
            std::getline(iss, amount_str, ',');

            bool negative = amount_str[1] == '-';
            auto idx = amount_str.find('.');

            long dollars, cents;
            std::from_chars(amount_str.c_str() + 1 + negative, amount_str.c_str() + idx, dollars);
            std::from_chars(amount_str.c_str() + idx + 1, amount_str.c_str() + amount_str.size(), cents);

            return (dollars * 100 + cents) * (negative ? -1 : 1);
        };

        auto user_id = next_as<uint16_t>(iss);
        auto card_id = next_as<uint8_t>(iss);
        auto year = next_as<uint16_t>(iss);
        auto month = next_as<uint8_t>(iss);
        auto day = next_as<uint8_t>(iss);
        auto hour = next_as<uint8_t>(iss, ':');
        auto minute = next_as<uint8_t>(iss);
        auto amount = get_amount();
        auto transaction_type = next_as<models::TransactionType>(iss);
        auto merchant_id = next_as<int64_t>(iss);
        auto merchant_city = next_as(iss);
        auto merchant_state = next_as(iss);
        auto merchant_zip = next_as<std::optional<uint32_t>>(iss);
        auto merchant_mcc = next_as<uint32_t>(iss);
        auto error_str = next_as(iss, ',', true);
        auto fraud = next_as<bool>(iss);

        std::vector<std::string> errors;

        std::string error;
        std::istringstream _iss{error_str};
        while (std::getline(_iss, error, ','))
            errors.push_back(error);

        struct tm date{};
        date.tm_year = year - 1900;
        date.tm_mon = month - 1;
        date.tm_mday = day;
        date.tm_hour = hour;
        date.tm_min = minute;
        time_t time = mktime(&date);

        return models::Transaction {
            user_id,
            card_id,
            time,
            amount,
            transaction_type,
            merchant_id,
            merchant_city,
            merchant_state,
            merchant_zip.has_value() ? *merchant_zip : 0,
            merchant_mcc,
            errors,
            fraud
        };
    }

    const Ref<http_response> query_transactions::process(const http_request& req)
    {
        auto& args = req.get_args();
        int total_count = -1;
        int transaction_count = -1;
        if (args.find("total_count") != args.end() && util::is_integer(args.at("total_count")))
        {
            auto count_str = args.at("total_count");
            std::from_chars(count_str.c_str(), count_str.c_str() + count_str.size(), total_count);
        }
        if (args.find("transaction_count") != args.end() && util::is_integer(args.at("transaction_count")))
        {
            auto count_str = args.at("transaction_count");
            std::from_chars(count_str.c_str(), count_str.c_str() + count_str.size(), transaction_count);
        }

        std::map<std::optional<uint32_t>, std::vector<models::Transaction>> transaction_by_zip;

        std::ifstream transactions("data/transactions.csv");
        std::string line;
        std::getline(transactions, line);
        while (std::getline(transactions, line))
        {
            std::istringstream iss{line};

            auto next_as_amount = [](auto& stream)
            {
                auto amount_str = next_as(stream);
                amount_str.erase(0, 1);
                bool negative = false;
                if (amount_str[0] == '-')
                {
                    negative = true;
                    amount_str.erase(0, 1);
                }

                std::string cent_str = amount_str;
                auto idx = amount_str.find('.');
                amount_str.erase(idx);
                cent_str.erase(0, idx + 1);

                long dollars, cents;
                std::from_chars(amount_str.c_str(), amount_str.c_str() + amount_str.size(), dollars);
                std::from_chars(cent_str.c_str(), cent_str.c_str() + cent_str.size(), cents);

                return (dollars * 100 + cents) *  (negative ? -1 : 1);
            };

            auto user_id = next_as<uint16_t>(iss);
            auto card_id = next_as<uint8_t>(iss);
            auto year = next_as<uint16_t>(iss);
            auto month = next_as<uint8_t>(iss);
            auto day = next_as<uint8_t>(iss);
            auto hour = next_as<uint8_t>(iss, ':');
            if (hour < 20)
                continue;
            auto minute = next_as<uint8_t>(iss);
            auto amount = next_as_amount(iss);
            auto transaction_type = next_as<models::TransactionType>(iss);
            auto merchant_id = next_as<int64_t>(iss);
            auto merchant_city = next_as(iss);
            auto merchant_state = next_as(iss);
            auto merchant_zip = next_as<std::optional<uint32_t>>(iss);
            auto merchant_mcc = next_as<uint32_t>(iss);
            auto error_str = next_as(iss, ',', true);
            auto fraud = next_as<bool>(iss);

            std::vector<std::string> errors;
            std::string error;
            std::istringstream _iss{error_str};

            while (std::getline(_iss, error, ','))
            {
                errors.push_back(error);
            }

            struct tm date{};
            date.tm_year = year - 1900;
            date.tm_mon = month - 1;
            date.tm_mday = day;
            date.tm_hour = hour;
            date.tm_min = minute;
            time_t time = mktime(&date);

            models::Transaction transaction {
                user_id,
                card_id,
                time,
                amount,
                transaction_type,
                merchant_id,
                merchant_city,
                merchant_state,
                merchant_zip.has_value() ? *merchant_zip : 0,
                merchant_mcc,
                errors,
                fraud
            };

            if (!transaction_by_zip.count(merchant_zip))
                transaction_by_zip.insert(std::make_pair(merchant_zip, std::vector<models::Transaction>()));
            transaction_by_zip[merchant_zip].push_back(std::move(transaction));
        }

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_child("Transactions", {{"GroupedBy", "zip"}});

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        int step = 0;
        for (const auto& [zip, transaction_list] : transaction_by_zip)
        {
            if (total_count != -1)
                step++;
            int transaction_step = 0;
            builder
                .add_array("Result", {{"Zip", zip.has_value() ? std::to_string(*zip) : "ONLINE"}}, transaction_list, [&](XmlBuilder& b, const models::Transaction& t) {
                    if (transaction_count == -1 || transaction_step < transaction_count)
                    {
                        b.add_child("Transaction");
                        // User/Card
                        {
                            uint16_t id = t.user_id;
                            MDB_val key{ sizeof(id), &id };
                            MDB_val result;

                            if (mdb_cursor_get(user_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                            {
                                auto* raw = (const uint8_t*)result.mv_data;
                                std::string first_name = next_as<std::string>(raw);
                                std::string last_name = next_as<std::string>(raw);
                                std::string email = next_as<std::string>(raw);

                                b
                                        .add_child("User", {{ "ID", std::to_string(id) }})
                                        .add_string("FirstName", first_name)
                                        .add_string("LastName", last_name)
                                        .add_string("Email", email)
                                        .step_up();
                            }

#pragma pack(push, 1)
                            struct card_key
                            {
                                uint16_t user_id;
                                uint8_t card_id;
                            };
#pragma pack(pop)

                            card_key card_id{ id, t.card_id };
                            key = MDB_val{ sizeof(card_id), &card_id };

                            if (mdb_cursor_get(card_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                            {
                                auto* raw = (const uint8_t*)result.mv_data;
                                auto type = next_as(raw);
                                std::string type_str;
                                switch (type)
                                {
                                case 0:
                                    type_str = "American Express";
                                    break;
                                case 1:
                                    type_str = "Visa";
                                    break;
                                case 2:
                                    type_str = "Mastercard";
                                    break;
                                default:
                                    type_str = "Unknown";
                                    break;
                                }
                                auto expire_month = next_as(raw);
                                auto expire_year = next_as(raw);
                                auto cvv = next_as<uint>(raw);
                                auto pan = next_as<std::string>(raw);

                                b
                                        .add_child("Card", {{ "ID", std::to_string(t.card_id) }})
                                        .add_string("CardType", type_str)
                                        .add_string("Expires",
                                                std::to_string(expire_month) + "/" + std::to_string(expire_year))
                                        .add_string("CVV", std::to_string(cvv))
                                        .add_string("PAN", pan)
                                        .step_up();
                            }
                        }

                        // Date
                        {
                            char mbstr[100];
                            std::strftime(mbstr, 100, "%T %m/%d/%Y", std::localtime(&t.time));
                            b
                                    .add_string("DateTime", std::string(mbstr));
                        }

                        // Transaction Type
                        {
                            switch (t.type)
                            {
                            case models::TransactionType::Chip:
                                b.add_string("TransactionType", "Chip Transaction");
                                break;
                            case models::TransactionType::Online:
                                b.add_string("TransactionType", "Online Transaction");
                                break;
                            case models::TransactionType::Swipe:
                                b.add_string("TransactionType", "Swipe Transaction");
                                break;
                            case models::TransactionType::Unknown:
                                b.add_string("TransactionType", "Unknown Transaction");
                                break;
                            }
                        }

                        // Merchant
                        {
                            int64_t merchant_id = t.merchant_id;
                            MDB_val key{ sizeof(merchant_id), &merchant_id };
                            MDB_val result;

                            if (mdb_cursor_get(merchant_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                            {
                                auto* raw = (const uint8_t*)result.mv_data;
                                auto name = next_as<std::string>(raw);
                                auto mcc = next_as<uint>(raw);
                                auto category = (models::MerchantCategory)next_as(raw);
                                std::string category_str;

                                switch (category)
                                {
                                case models::MerchantCategory::Agricultural:
                                    category_str = "Agricultural";
                                    break;
                                case models::MerchantCategory::Contracted:
                                    category_str = "Contracted";
                                    break;
                                case models::MerchantCategory::TravelAndEntertainment:
                                    category_str = "Travel and Entertainment";
                                    break;
                                case models::MerchantCategory::CarRental:
                                    category_str = "Car Rental";
                                    break;
                                case models::MerchantCategory::Lodging:
                                    category_str = "Lodging";
                                    break;
                                case models::MerchantCategory::Transportation:
                                    category_str = "Transportation";
                                    break;
                                case models::MerchantCategory::Utility:
                                    category_str = "Utility";
                                    break;
                                case models::MerchantCategory::RetailOutlet:
                                    category_str = "Retail Outlet";
                                    break;
                                case models::MerchantCategory::ClothingStore:
                                    category_str = "Clothing Store";
                                    break;
                                case models::MerchantCategory::MiscStore:
                                    category_str = "Miscellaneous Store";
                                    break;
                                case models::MerchantCategory::Business:
                                    category_str = "Business";
                                    break;
                                case models::MerchantCategory::ProfessionalOrMembership:
                                    category_str = "Professional or Membership";
                                    break;
                                case models::MerchantCategory::Government:
                                    category_str = "Government";
                                    break;
                                }

                                b
                                        .add_child("Merchant", {{ "ID", std::to_string(merchant_id) }})
                                        .add_string("Name", name)
                                        .add_string("MCC", std::to_string(mcc))
                                        .add_string("BusinessCategory", category_str)
                                        .step_up();
                            }
                        }

                        // Location
                        {
                            b
                                    .add_string("City", t.merchant_city)
                                    .add_string("State", t.merchant_state);

                            if (t.zip != 0)
                                b.add_string("Zip", std::to_string(t.zip));
                        }

                        // Errors
                        {
                            b
                                    .add_array("Errors", t.errors, [](XmlBuilder& b, const auto& e)
                                    {
                                        b.add_string("Error", e);
                                    });
                        }

                        // Is Fraud?
                        {
                            b
                                    .add_string("IsFraud", t.is_fraud ? "Yes" : "No");
                        }
                        b.step_up();
                    }
                    if (transaction_count != -1)
                        transaction_step++;
                });
            if (step >= total_count)
                break;
        }

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }

    const Ref<http_response> total_fraud_free_transactions::process(const http_request& req)
    {
        std::unordered_map<std::string, int> transaction_count_by_state;

        std::ifstream transactions("data/transactions.csv");
        std::string line;
        std::getline(transactions, line);
        while (std::getline(transactions, line))
        {
            std::istringstream iss{line};
            auto skip = [&iss](int count) {
                std::string _;
                for (int i = 0; i < count; ++i)
                    std::getline(iss, _, ',');
            };
            auto skip_quoted = [&iss]() {
                std::string _a;
                do
                {
                    std::string _b;
                    std::getline(iss, _b, ',');
                    _a += _b;
                }
                while (!_a.empty() && _a[_a.size() - 1] != '"');
            };

            skip(10);
            std::string state_abbrev; std::getline(iss, state_abbrev, ',');
            skip(2);
            skip_quoted();
            std::string fraud_str; std::getline(iss, fraud_str, ',');
            if (fraud_str != "No")
                continue;

            if (!transaction_count_by_state.count(state_abbrev))
                transaction_count_by_state.emplace(state_abbrev, 1);
            else
                transaction_count_by_state[state_abbrev]++;
        }

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto state_dbi = lmdb::dbi::open(rtxn, "states");
        auto state_cursor = lmdb::cursor::open(rtxn, state_dbi);

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
            .add_iterator("FraudFreeTransactions", {{"GroupedBy", "state"}}, transaction_count_by_state.begin(), transaction_count_by_state.end(), [&](XmlBuilder& b, const auto& pair) {
                auto [state_abbrev, count] = pair;
                MDB_val key{state_abbrev.size(), (void*)state_abbrev.c_str()};
                MDB_val result;

                if (mdb_cursor_get(state_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                {
                    auto* raw = (const uint8_t*)result.mv_data;
                    std::string name = next_as<std::string>(raw);

                    b
                        .add_child("Result")
                            .add_string("State", name)
                            .add_string("Count", std::to_string(count))
                        .step_up();
                }
            });

        state_cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }

    const Ref<http_response> top_10_largest_transactions::process(const http_request& req)
    {
        std::array<std::pair<long, models::Transaction>, 10> top10;
        int filled = 0;

        std::ifstream transactions("data/transactions.csv");
        std::string line;
        std::getline(transactions, line);
        while (std::getline(transactions, line))
        {
            std::istringstream iss{line};
            auto skip = [&iss](int count) {
                std::string _;
                for (int i = 0; i < count; ++i)
                    std::getline(iss, _, ',');
            };
            auto get_amount = [&iss, &skip]() -> long {
                skip(6);
                std::string amount_str;
                std::getline(iss, amount_str, ',');

                bool negative = amount_str[1] == '-';
                auto idx = amount_str.find('.');

                long dollars, cents;
                std::from_chars(amount_str.c_str() + 1 + negative, amount_str.c_str() + idx, dollars);
                std::from_chars(amount_str.c_str() + idx + 1, amount_str.c_str() + amount_str.size(), cents);

                return (dollars * 100 + cents) * (negative ? -1 : 1);
            };

            auto amount = get_amount();
            if (filled == 10 && amount < top10[top10.size()-1].first)
                continue;

            auto transaction = parse_transaction(line);
            if (transaction.is_fraud || !transaction.errors.empty())
                continue;

            if (filled < 10)
            {
                top10[(size_t)filled] = std::make_pair(amount, transaction);
                std::sort(top10.begin(), top10.begin() + filled + 1, [](const auto& p1, const auto& p2) { return p1.first > p2.first; });
                filled++;
            }
            else
            {
                top10[top10.size()-1] = std::make_pair(amount, transaction);
                std::sort(top10.begin(), top10.end(), [](const auto& p1, const auto& p2) { return p1.first > p2.first; });
            }
        }

        auto rtxn = lmdb::txn::begin(*p_env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_iterator("Results", {{"count", "10"}, {"orderBy", "transaction_size"}, {"orderDir", "descending"}}, top10.begin(), top10.end(), [&](XmlBuilder& b, const auto& pair) {
                    b.add_child("Result").add_child("Transaction");
                    models::Transaction t = pair.second;
                    // Amount
                    {
                        long dollars, cents;
                        dollars = t.amount / 100;
                        cents = t.amount < 0 ? (t.amount * -1) % 100 : t.amount % 100;
                        std::ostringstream ss;
                        ss << "$" << dollars << "." << std::setw(2) << cents;
                        b.add_string("Amount", ss.str());
                    }

                    // User/Card
                    {
                        MDB_val key{sizeof(t.user_id), &t.user_id};
                        MDB_val result;

                        if (mdb_cursor_get(user_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                        {
                            auto* raw = (const uint8_t*)result.mv_data;
                            std::string first_name = next_as<std::string>(raw);
                            std::string last_name = next_as<std::string>(raw);
                            std::string email = next_as<std::string>(raw);

                            b
                                    .add_child("User", {{ "id", std::to_string(t.user_id) }})
                                    .add_string("FirstName", first_name)
                                    .add_string("LastName", last_name)
                                    .add_string("Email", email);
                        }

                        key = MDB_val{sizeof(t.user_id) + sizeof(t.card_id), &t.user_id};

                        if (mdb_cursor_get(card_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                        {
                            auto* raw = (const uint8_t*)result.mv_data;
                            auto type = next_as(raw);
                            std::string type_str;
                            switch (type)
                            {
                            case 0:
                                type_str = "American Express";
                                break;
                            case 1:
                                type_str = "Visa";
                                break;
                            case 2:
                                type_str = "Mastercard";
                                break;
                            default:
                                type_str = "Unknown";
                                break;
                            }
                            auto expire_month = next_as(raw);
                            auto expire_year = next_as(raw);
                            auto cvv = next_as<uint>(raw);
                            auto pan = next_as<std::string>(raw);

                            b
                                    .add_child("Card", {{ "id", std::to_string(t.card_id) }})
                                    .add_string("CardType", type_str)
                                    .add_string("Expires",
                                            std::to_string(expire_month) + "/" + std::to_string(expire_year))
                                    .add_string("CVV", std::to_string(cvv))
                                    .add_string("PAN", pan)
                                    .step_up()
                                    .step_up();
                        }
                    }

                    // Date
                    {
                        char mbstr[100];
                        std::strftime(mbstr, 100, "%T %m/%d/%Y", std::localtime(&t.time));
                        b
                            .add_string("DateTime", std::string(mbstr));
                    }

                    // Transaction Type
                    {
                        switch (t.type)
                        {
                        case models::TransactionType::Chip:
                            b.add_string("TransactionType", "Chip Transaction");
                            break;
                        case models::TransactionType::Online:
                            b.add_string("TransactionType", "Online Transaction");
                            break;
                        case models::TransactionType::Swipe:
                            b.add_string("TransactionType", "Swipe Transaction");
                            break;
                        case models::TransactionType::Unknown:
                            b.add_string("TransactionType", "Unknown Transaction");
                            break;
                        }
                    }

                    // Merchant
                    {
                        MDB_val key{sizeof(t.merchant_id), &t.merchant_id};
                        MDB_val result;

                        if (mdb_cursor_get(merchant_cursor, &key, &result, MDB_SET) == MDB_SUCCESS)
                        {
                            auto* raw = (const uint8_t*)result.mv_data;
                            auto name = next_as<std::string>(raw);
                            auto mcc = next_as<uint>(raw);
                            auto category = (models::MerchantCategory)next_as(raw);
                            std::string category_str;

                            switch (category)
                            {
                            case models::MerchantCategory::Agricultural:
                                category_str = "Agricultural";
                                break;
                            case models::MerchantCategory::Contracted:
                                category_str = "Contracted";
                                break;
                            case models::MerchantCategory::TravelAndEntertainment:
                                category_str = "Travel and Entertainment";
                                break;
                            case models::MerchantCategory::CarRental:
                                category_str = "Car Rental";
                                break;
                            case models::MerchantCategory::Lodging:
                                category_str = "Lodging";
                                break;
                            case models::MerchantCategory::Transportation:
                                category_str = "Transportation";
                                break;
                            case models::MerchantCategory::Utility:
                                category_str = "Utility";
                                break;
                            case models::MerchantCategory::RetailOutlet:
                                category_str = "Retail Outlet";
                                break;
                            case models::MerchantCategory::ClothingStore:
                                category_str = "Clothing Store";
                                break;
                            case models::MerchantCategory::MiscStore:
                                category_str = "Miscellaneous Store";
                                break;
                            case models::MerchantCategory::Business:
                                category_str = "Business";
                                break;
                            case models::MerchantCategory::ProfessionalOrMembership:
                                category_str = "Professional or Membership";
                                break;
                            case models::MerchantCategory::Government:
                                category_str = "Government";
                                break;
                            }

                            b
                                    .add_child("Merchant", {{ "ID", std::to_string(t.merchant_id) }})
                                    .add_string("Name", name)
                                    .add_string("MCC", std::to_string(mcc))
                                    .add_string("BusinessCategory", category_str)
                                    .step_up();
                        }
                    }

                    // Location
                    {
                        if (t.zip != 0)
                        {
                            b
                                .add_string("City", t.merchant_city)
                                .add_string("State", t.merchant_state)
                                .add_string("Zip", std::to_string(t.zip));
                        }
                        else if (t.zip == 0 && !t.merchant_state.empty())
                        {
                            b
                                .add_string("Country", t.merchant_state);
                        }
                    }
                    b.step_up().step_up();
                });

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        return std::make_shared<string_response>(builder.serialize(), 200, "application/xml");
    }
}