#include "resources.hpp"

#include <fstream>
#include <charconv>
#include <optional>
#include <iomanip>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "models.hpp"
#include "helpers/xml_builder.hpp"
#include "helpers/utilities.hpp"

using namespace std::literals;

namespace resources::analytics
{
    using httpserver::string_response;

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

    struct User
    {
        uint16_t id;
        std::string first_name;
        std::string last_name;
        std::string email;

        struct Card
        {
            uint8_t id;
            std::string type;
            uint8_t expire_month;
            uint8_t expire_year;

            uint cvv;
            std::string pan;

            Card() = default;

            Card(uint16_t uid, uint8_t cid, lmdb::cursor& cc)
            {
#pragma pack(push, 1)
                struct
                {
                    uint16_t user;
                    uint8_t card;
                } cardKey { uid, cid };
#pragma pack(pop)

                MDB_val key{sizeof(cardKey), &cardKey};
                MDB_val val;
                mdb_cursor_get(cc, &key, &val, MDB_SET);
                auto* raw = (const uint8_t*)val.mv_data;
                id = cid;
                auto t = next_as(raw);
                switch (t)
                {
                    case 0: type = "American Express"; break;
                    case 1: type = "Visa"; break;
                    case 2: type = "Mastercard"; break;
                    default: type = "Unknown"; break;
                }
                expire_month = next_as(raw);
                expire_year = next_as(raw);
                cvv = next_as<uint>(raw);
                pan = next_as<std::string>(raw);
            }
        } card;

        User() = default;

        User(uint16_t uid, uint8_t cid, lmdb::cursor& uc, lmdb::cursor& cc)
            : card(uid, cid, cc)
        {
            MDB_val key{sizeof(uid), &uid};
            MDB_val val;
            mdb_cursor_get(uc, &key, &val, MDB_SET);
            auto* raw = (const uint8_t*)val.mv_data;
            id = uid;
            first_name = next_as<std::string>(raw);
            last_name = next_as<std::string>(raw);
            email = next_as<std::string>(raw);
        }

        static std::map<std::pair<uint16_t, uint8_t>, User> cache;

        static User& get(uint16_t uid, uint8_t cid, lmdb::cursor& uc, lmdb::cursor& cc)
        {
            if (!cache.count(std::make_pair(uid, cid)))
                cache.emplace(std::make_pair(uid, cid), User(uid, cid, uc, cc));

            return cache[std::make_pair(uid, cid)];
        }
    };
    std::map<std::pair<uint16_t, uint8_t>, User> User::cache{};

    struct Location
    {
        bool online;
        bool foreign;
        uint32_t zip;
        std::string city;
        std::string state;

        static Location from_online(bool online)
        {
            return { online, false, 0, std::string{}, std::string{} };
        }
        static Location from_foreign(bool foreign)
        {
            return { false, foreign, 0, std::string{}, std::string{} };
        }
        static Location from_zip(uint32_t zip)
        {
            return { false, false, zip, std::string{}, std::string{} };
        }
        static Location from_city(std::string city)
        {
            return { false, false, 0, std::move(city), std::string{} };
        }
        static Location from_state(std::string state)
        {
            return { false, false, 0, std::string{}, std::move(state) };
        }
    };

    struct Merchant
    {
        int64_t id{};
        std::string name;
        uint mcc{};
        std::string category;
        std::vector<Location> locations;

        static std::map<int64_t, Merchant> cache;

        Merchant() = default;

        Merchant(int64_t mid, lmdb::cursor& mc)
        {
            MDB_val key{sizeof(mid), &mid};
            MDB_val val;
            mdb_cursor_get(mc, &key, &val, MDB_SET);
            auto* raw = (const uint8_t*)val.mv_data;
            id = mid;
            name = next_as<std::string>(raw);
            mcc = next_as<uint>(raw);
            auto c = (models::MerchantCategory)next_as(raw);
            switch (c)
            {
                case models::MerchantCategory::Agricultural: category = "Agricultural"; break;
                case models::MerchantCategory::Contracted: category = "Contracted"; break;
                case models::MerchantCategory::TravelAndEntertainment: category = "Travel and Entertainment"; break;
                case models::MerchantCategory::CarRental: category = "Car Rental"; break;
                case models::MerchantCategory::Lodging: category = "Lodging"; break;
                case models::MerchantCategory::Transportation: category = "Transportation"; break;
                case models::MerchantCategory::Utility: category = "Utility"; break;
                case models::MerchantCategory::RetailOutlet: category = "Retail Outlet"; break;
                case models::MerchantCategory::ClothingStore: category = "Clothing Store"; break;
                case models::MerchantCategory::MiscStore: category = "Miscellaneous Store"; break;
                case models::MerchantCategory::Business: category = "Business"; break;
                case models::MerchantCategory::ProfessionalOrMembership: category = "Professional or Membership"; break;
                case models::MerchantCategory::Government: category = "Government"; break;
            }

            auto size = next_as<uint64_t>(raw);
            locations.reserve(size);
            for (uint64_t i = 0; i < size; ++i)
            {
                auto online = next_as<uint8_t>(raw) == 1;
                auto foreign = next_as<uint8_t>(raw) == 1;
                auto zip = next_as<uint32_t>(raw);
                auto city = next_as<std::string>(raw);
                auto state = next_as<std::string>(raw);

                locations.push_back({ online, foreign, zip, city, state });
            }
        }

        static Merchant& get(int64_t mid, lmdb::cursor& mc)
        {
            if (!cache.count(mid))
                cache.emplace(mid, Merchant {mid, mc});

            return cache[mid];
        }
    };
    std::map<int64_t, Merchant> Merchant::cache{};

    enum class Order
    {
        Descending,
        Ascending
    };

    enum class TransactionField
    {
        UserID,
        UserFirstName,
        UserLastName,
        UserEmail,
        CardID,
        CardType,
        CardExpires,
        CardCVV,
        CardPan,
        Time,
        Amount,
        Type,
        MerchantID,
        MerchantName,
        MerchantCategory,
        MerchantCity,
        MerchantState,
        MerchantZip,
        MerchantOnline,
        MerchantForeign,
        City,
        State,
        Zip,
        MCC,
        Error,
        Fraudulent
    };

    enum class SelectorType
    {
        IsEqual,
        IsNotEqual,
        InRange,
        IsNotInRange,
        IsOneOf,
        IsNotOneOf,
        Contains,
        ContainsOnly,
        ContainsOneOf,
        ContainsAllOf,
        ContainsNoneOf,
        LessThan,
        LessThanEqual,
        GreaterThan,
        GreaterThanEqual
    };

    struct SelectorValidation
    {
        bool valid;
        std::string error;
    };

    struct QuerySelector
    {
        TransactionField field;
        SelectorType type;
        std::vector<std::string> values;
    };

    enum PropertyCondition
    {
        OneOrMore
    };

    struct QueryProperty
    {
        PropertyCondition condition;
        QuerySelector selector;
    };

    TransactionField transaction_field_from_json(const nlohmann::json& j)
    {
        if (j.is_string())
        {
            auto str = j.get<std::string>();
            if (str == "user_id") return TransactionField::UserID;
            else if (str == "card_id") return TransactionField::CardID;
            else if (str == "amount") return TransactionField::Amount;
            else if (str == "type") return TransactionField::Type;
            else if (str == "merchant_id") return TransactionField::MerchantID;
            else if (str == "city") return TransactionField::City;
            else if (str == "state") return TransactionField::State;
            else if (str == "zip") return TransactionField::Zip;
            else if (str == "mcc") return TransactionField::MCC;
            else if (str == "error") return TransactionField::Error;
            else if (str == "fraudulent") return TransactionField::Fraudulent;
        }
        else
        {
            if (j.contains("user"))
            {
                auto str = j["user"].get<std::string>();
                if (str == "id") return TransactionField::UserID;
                else if (str == "first_name") return TransactionField::UserFirstName;
                else if (str == "last_name") return TransactionField::UserLastName;
                else if (str == "email") return TransactionField::UserEmail;
            }
            else if (j.contains("card"))
            {
                auto str = j["card"].get<std::string>();
                if (str == "id") return TransactionField::CardID;
                else if (str == "type") return TransactionField::CardType;
                else if (str == "expires") return TransactionField::CardExpires;
                else if (str == "cvv") return TransactionField::CardCVV;
                else if (str == "pan") return TransactionField::CardPan;
            }
            else if (j.contains("merchant"))
            {
                auto str = j["merchant"].get<std::string>();
                if (str == "id") return TransactionField::MerchantID;
                else if (str == "name") return TransactionField::MerchantName;
                else if (str == "mcc") return TransactionField::MCC;
                else if (str == "category") return TransactionField::MerchantCategory;
                else if (str == "city") return TransactionField::MerchantCity;
                else if (str == "state") return TransactionField::MerchantState;
                else if (str == "online") return TransactionField::MerchantOnline;
                else if (str == "foreign") return TransactionField::MerchantForeign;
                else if (str == "zip") return TransactionField::MerchantZip;
            }
        }

        throw std::invalid_argument("Invalid transaction");
    }

    SelectorType selector_type_from_string(const std::string_view& str)
    {
        if      (str == "isEqual")  return SelectorType::IsEqual;
        else if (str == "isNotEqual")   return SelectorType::IsNotEqual;
        else if (str == "inRange")      return SelectorType::InRange;
        else if (str == "isNotInRange") return SelectorType::IsNotInRange;
        else if (str == "isOneOf")      return SelectorType::IsOneOf;
        else if (str == "isNotOneOf")   return SelectorType::IsNotOneOf;
        else if (str == "contains")     return SelectorType::Contains;
        else if (str == "containsOnly") return SelectorType::ContainsOnly;
        else if (str == "containsOneOf")    return SelectorType::ContainsOneOf;
        else if (str == "containsAllOf")    return SelectorType::ContainsAllOf;
        else if (str == "containsNoneOf")   return SelectorType::ContainsNoneOf;
        else if (str == "lessThan")         return SelectorType::LessThan;
        else if (str == "lessThanEqual")    return SelectorType::LessThanEqual;
        else if (str == "greaterThan")      return SelectorType::GreaterThan;
        else if (str == "greaterThanEqual") return SelectorType::GreaterThanEqual;
        else throw std::invalid_argument("Invalid selector");
    }

    PropertyCondition property_condition_from_string(const std::string_view& str)
    {
        if (str == "oneOrMore") return PropertyCondition::OneOrMore;
        else throw std::invalid_argument("invalid condition");
    }

    std::string_view transaction_type_to_selector(const models::TransactionType& type)
    {
        switch (type)
        {
            case models::TransactionType::Chip: return "chip";
            case models::TransactionType::Online: return "online";
            case models::TransactionType::Swipe: return "swipe";
            case models::TransactionType::Unknown: return "unknown";
        }
    }

    SelectorValidation validate_selector(const QuerySelector& selector)
    {
        switch (selector.field)
        {
            case TransactionField::UserID:
            case TransactionField::UserFirstName:
            case TransactionField::UserLastName:
            case TransactionField::UserEmail:
            case TransactionField::CardID:
            case TransactionField::CardPan:
            case TransactionField::CardType:
            case TransactionField::CardCVV:
            case TransactionField::MerchantID:
            case TransactionField::MerchantName:
            case TransactionField::MerchantCategory:
            case TransactionField::City:
            case TransactionField::State:
            case TransactionField::Type:
            case TransactionField::Fraudulent:
            {
                switch (selector.type)
                {
                    case SelectorType::IsEqual:
                    case SelectorType::IsNotEqual:
                        if (selector.values.size() != 1)
                            return { false, "Selector can only be matched against a single value!" };
                        break;
                    case SelectorType::IsOneOf:
                    case SelectorType::IsNotOneOf:
                        if (selector.values.size() < 2)
                            return { false, "Selector must be matched against two or more values!" };
                        break;
                    default:
                        return { false, "Field can only be matched against exact values!" };
                }
                break;
            }
            case TransactionField::Amount:
            case TransactionField::Time:
            case TransactionField::CardExpires:
            case TransactionField::Zip:
            case TransactionField::MCC:
            {
                switch (selector.type)
                {
                    case SelectorType::IsEqual:
                    case SelectorType::IsNotEqual:
                    case SelectorType::LessThan:
                    case SelectorType::LessThanEqual:
                    case SelectorType::GreaterThan:
                    case SelectorType::GreaterThanEqual:
                    {
                        if (selector.values.size() != 1)
                            return { false, "Selector can only be matched against a single value!" };
                        break;
                    }
                    case SelectorType::InRange:
                    case SelectorType::IsNotInRange:
                    {
                        if (selector.values.size() % 2 != 0)
                            return { false, "Selector must be matched against pairs of values!" };
                        break;
                    }
                    case SelectorType::IsOneOf:
                    case SelectorType::IsNotOneOf:
                    {
                        if (selector.values.size() < 2)
                            return { false, "Selector must be matched against two or more values!" };
                        break;
                    }
                    default:
                        return { false, "Field can't be matched against container selectors!" };
                }
                break;
            }
            case TransactionField::MerchantCity:
            case TransactionField::MerchantState:
            case TransactionField::MerchantOnline:
            case TransactionField::MerchantForeign:
            case TransactionField::MerchantZip:
            case TransactionField::Error:
            {
                switch (selector.type)
                {
                    case SelectorType::Contains:
                    case SelectorType::ContainsOnly:
                    {
                        if (selector.values.size() != 1)
                            return { false, "Selector can only be matched against a single value!" };
                        break;
                    }
                    case SelectorType::ContainsOneOf:
                    case SelectorType::ContainsAllOf:
                    case SelectorType::ContainsNoneOf:
                    {
                        if (selector.values.size() < 2)
                            return { false, "Selector must be matched against two or more values!" };
                        break;
                    }
                    default:
                        return { false, "Field can only be matched against container selectors!" };
                }
            }
        }

        return { true, std::string{} };
    }

    struct TransactionQueryOptions
    {
        int count = -1;
        Order order = Order::Descending;
        bool verbose = false;
        bool strict = false;
        bool pretty = false;
        std::vector<QuerySelector> selectors;
        std::vector<QueryProperty> properties;
    };

    using TransactionList = std::vector<models::Transaction>;

    template<typename T>
    bool check_field_against_selector(const QuerySelector& selector, T field)
    {
        if constexpr(util::is_vector_v<T>)
        {
            using Type = util::vector_type_t<T>;
            std::vector<Type> values = util::vector_map(selector.values, [&selector](const std::string& v) {
                (void)selector;
                if constexpr(std::is_same_v<std::string, Type> || std::is_same_v<std::string_view, Type>)
                {
                    std::string res = v;
                    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
                    return res;
                }
                else if constexpr(std::is_same_v<Location, Type>)
                {
                    switch (selector.field)
                    {
                        case TransactionField::MerchantCity:
                            return Location::from_city(util::to_lower(v));
                        case TransactionField::MerchantState:
                            return Location::from_state(util::to_lower(v));
                        case TransactionField::MerchantZip:
                        {
                            auto [ec, value] { util::parse<uint32_t>(util::to_lower(v)) };
                            if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
                                throw std::runtime_error("Selector values for Zip must be an integer");
                            return Location::from_zip(value);
                        }
                        case TransactionField::MerchantForeign:
                        {
                            auto [ec, value] { util::parse<bool>(util::to_lower(v)) };
                            if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
                                throw std::runtime_error("Selector values for Foreign must be a boolean");
                            return Location::from_foreign(value);
                        }
                        case TransactionField::MerchantOnline:
                        {
                            auto [ec, value] { util::parse<bool>(util::to_lower(v)) };
                            if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
                                throw std::runtime_error("Selector values for Online must be a boolean");
                            return Location::from_online(value);
                        }
                        default: throw std::runtime_error("Weird field after validation?");
                    }
                }
                else
                {
                    auto [ec, value] { util::parse<Type>(v) };
                    if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
                        throw std::runtime_error("Selector values for UserIds must be an integer");
                    return value;
                }
            });

            auto isEqual = [&selector](const auto& f, const auto& v) {
                (void)selector;

                if constexpr(std::is_same_v<Type, Location>)
                {
                    switch (selector.field)
                    {
                        case TransactionField::MerchantCity:
                            return f.city == v.city;
                        case TransactionField::MerchantState:
                            return f.state == v.state;
                        case TransactionField::MerchantZip:
                            return f.zip == v.zip;
                        case TransactionField::MerchantForeign:
                            return f.foreign == v.foreign;
                        case TransactionField::MerchantOnline:
                            return f.online == v.online;
                        default: throw std::runtime_error("Weird field after validation?");
                    }
                }
                else
                {
                    return f == v;
                }
            };

            auto contains = [&values,&isEqual](const auto& f) { return isEqual(f, values[0]); };
            auto containsOneOf = [&values,&isEqual](const auto& f) {
                return std::any_of(values.begin(), values.end(), [&f,&isEqual](const auto& v) { return isEqual(v, f); });
            };
            auto isInVec = [&field,&isEqual](const auto& v) {
                return std::find_if(field.begin(), field.end(), [&](const auto& f) { return isEqual(f, v); }) != field.end();
            };

            switch (selector.type)
            {
                case SelectorType::Contains: return std::any_of(field.begin(), field.end(), contains);
                case SelectorType::ContainsOnly: return field.size() == 1 && contains(field[0]);
                case SelectorType::ContainsOneOf: return std::any_of(field.begin(), field.end(), containsOneOf);
                case SelectorType::ContainsAllOf: return std::all_of(values.begin(), values.end(), isInVec);
                case SelectorType::ContainsNoneOf: return std::none_of(values.begin(), values.end(), isInVec);
                default: return false;
            }
        }
        else
        {
            std::vector<util::promote_t<T>> values = util::vector_map(selector.values, [](const std::string& v) {
                if constexpr(std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>)
                {
                    std::string res = v;
                    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
                    return res;
                }
                else if constexpr(std::is_same_v<bool, T>)
                {
                    std::string res = v;
                    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
                    if (res == "true" || res == "1")
                        return true;
                    else if (res == "false" || res == "0")
                        return false;
                    throw std::runtime_error("Selector values didn't match type of field!");
                }
                else
                {
                    auto [ec, value] { util::parse<T>(v) };
                    if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
                        throw std::runtime_error("Selector values didn't match type of field!");
                    return value;
                }
            });

            auto equals = [&field](const auto& v) { return v == field; };

            switch (selector.type)
            {
                case SelectorType::IsEqual: return field == values[0];
                case SelectorType::IsNotEqual: return field != values[0];
                case SelectorType::InRange: return values[0] <= field && field <= values[1];
                case SelectorType::IsNotInRange: return !(values[0] <= field && field <= values[1]);
                case SelectorType::IsOneOf: std::any_of(values.begin(), values.end(), equals);
                case SelectorType::IsNotOneOf: std::none_of(values.begin(), values.end(), equals);
                case SelectorType::LessThan: return field < values[0];
                case SelectorType::LessThanEqual: return field <= values[0];
                case SelectorType::GreaterThan: return field > values[0];
                case SelectorType::GreaterThanEqual: return field >= values[0];
                default: return false;
            }
        }
    }

    bool should_skip_transaction(const models::Transaction& transaction, lmdb::cursor& user_cursor, lmdb::cursor& card_cursor, lmdb::cursor& merchant_cursor, bool strict, const std::vector<QuerySelector>& selectors)
    {
        if (strict && (transaction.is_fraud || !transaction.errors.empty()))
            return true;

        auto user = User::get(transaction.user_id, transaction.card_id, user_cursor, card_cursor);
        auto merchant = Merchant::get(transaction.merchant_id, merchant_cursor);

        for (const auto& selector : selectors)
        {
            switch (selector.field)
            {
                case TransactionField::UserID:
                    return !check_field_against_selector(selector, transaction.user_id);
                case TransactionField::UserFirstName:
                    return !check_field_against_selector(selector, user.first_name);
                case TransactionField::UserLastName:
                    return !check_field_against_selector(selector, user.last_name);
                case TransactionField::UserEmail:
                    return !check_field_against_selector(selector, user.email);
                case TransactionField::CardID:
                    return !check_field_against_selector(selector, transaction.card_id);
                case TransactionField::CardType:
                    return !check_field_against_selector(selector, user.card.type);
                case TransactionField::CardCVV:
                    return !check_field_against_selector(selector, user.card.cvv);
                case TransactionField::CardPan:
                    return !check_field_against_selector(selector, user.card.pan);
                case TransactionField::Amount:
                    return !check_field_against_selector(selector, transaction.amount);
                case TransactionField::Type:
                    return !check_field_against_selector(selector, transaction_type_to_selector(transaction.type));
                case TransactionField::MerchantID:
                    return !check_field_against_selector(selector, transaction.merchant_id);
                case TransactionField::MerchantName:
                    return !check_field_against_selector(selector, merchant.name);
                case TransactionField::MerchantCategory:
                    return !check_field_against_selector(selector, merchant.category);
                case TransactionField::MerchantCity:
                case TransactionField::MerchantState:
                case TransactionField::MerchantZip:
                case TransactionField::MerchantForeign:
                case TransactionField::MerchantOnline:
                    return !check_field_against_selector(selector, merchant.locations);
                case TransactionField::City:
                    return !check_field_against_selector(selector, transaction.merchant_city);
                case TransactionField::State:
                    return !check_field_against_selector(selector, transaction.merchant_state);
                case TransactionField::Zip:
                    return !check_field_against_selector(selector, transaction.zip);
                case TransactionField::MCC:
                    return !check_field_against_selector(selector, transaction.mcc);
                case TransactionField::Error:
                    return !check_field_against_selector(selector, transaction.errors);
                case TransactionField::Fraudulent:
                    return !check_field_against_selector(selector, transaction.is_fraud);
            }
        }

        return false;
    }

    bool validate_list_against_properties(const TransactionList& transactions, lmdb::cursor& user_cursor, lmdb::cursor& card_cursor, lmdb::cursor& merchant_cursor, const std::vector<QueryProperty>& properties)
    {
        for (const auto& property : properties)
        {
            if (property.condition == PropertyCondition::OneOrMore)
            {
                for (const auto& transaction : transactions)
                {
                    if (!should_skip_transaction(transaction, user_cursor, card_cursor, merchant_cursor, false, { property.selector }))
                        return true;
                }
                return false;
            }
        }

        return true;
    }

    void serialize_amount(XmlBuilder& b, long amount) {
        long dollars, cents;
        dollars = amount / 100;
        cents = amount < 0 ? (amount * -1) % 100 : amount % 100;
        std::ostringstream ss;
        ss << "$" << dollars << "." << std::setw(2) << cents;
        b.add_string("Amount", ss.str());
    }
    void serialize_user_card(XmlBuilder& b, uint16_t user_id, uint8_t card_id) {
        b.add_empty("User", {
                {"id", std::to_string(user_id)},
                {"card", std::to_string(card_id)},
        });
    }
    void serialize_user_card(XmlBuilder& b, uint16_t user_id, uint8_t card_id, lmdb::cursor& user_cursor, lmdb::cursor& card_cursor)
    {
        auto u = User::get(user_id, card_id, user_cursor, card_cursor);
        b
            .add_child("User", {{"id", std::to_string(user_id)}})
                .add_string("FirstName", u.first_name)
                .add_string("LastName", u.last_name)
                .add_string("Email", u.email)
                .add_child("Card", {{"id", std::to_string(card_id)}})
                    .add_string("CardType", u.card.type)
                    .add_string("Expires",
                        std::to_string(u.card.expire_month) + "/" + std::to_string(u.card.expire_year))
                    .add_string("CVV", std::to_string(u.card.cvv))
                    .add_string("PAN", u.card.pan)
                .step_up()
            .step_up();
    }
    void serialize_date(XmlBuilder& b, time_t date) {
        char mb_str[100];
        std::strftime(mb_str, 100, "%T %m/%d/%Y", std::localtime(&date));
        b
                .add_string("DateTime", std::string(mb_str));
    }
    void serialize_transaction_type(XmlBuilder& b, models::TransactionType type) {
        switch (type)
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
    void serialize_merchant(XmlBuilder& b, int64_t merchant_id, uint mcc) {
        b
            .add_empty("Merchant", {
                    {"id", std::to_string(merchant_id)},
                    {"mcc", std::to_string(mcc)}
            });
    }
    void serialize_merchant(XmlBuilder& b, int64_t merchant_id, lmdb::cursor& merchant_cursor)
    {
        auto m = Merchant::get(merchant_id, merchant_cursor);
        b
            .add_child("Merchant", {{ "id", std::to_string(m.id) }})
                .add_string("Name", m.name)
                .add_string("MCC", std::to_string(m.mcc))
                .add_string("BusinessCategory", m.category)
            .step_up();
    }
    void serialize_location(XmlBuilder& b, const std::string& city, const std::string& state, uint32_t zip) {
        if (zip != 0)
        {
            b
                    .add_empty("Location", {
                            {"city", city},
                            {"state", state},
                            {"zip", std::to_string(zip)}
                    });
        }
        else if (zip == 0 && !state.empty())
        {
            b.add_empty("Location", {{ "country", state }});
        }
    }

    struct sort_by_amount
    {
        Order order;

        bool operator()(const models::Transaction& a, const models::Transaction& b) const
        {
            return order == Order::Descending ?
                   std::greater<long>{}(a.amount, b.amount) :
                   std::less<long>{}(a.amount, b.amount);
        }
    };

    template<typename T>
    struct sort_by_count
    {
        Order order;

        using comparisonType = std::pair<T, TransactionList>;

        bool operator()(const comparisonType& a, const comparisonType& b) const
        {
            return order == Order::Descending ?
                std::greater<size_t>{}(a.second.size(), b.second.size()) :
                std::less<size_t>{}(a.second.size(), b.second.size());
        }
    };

    template<typename Key, typename Sort = sort_by_count<Key>>
    using count_set = std::set<std::pair<Key, TransactionList>, Sort>;

    const Ref<http_response> process_cities(TransactionQueryOptions& options, lmdb::env& env, bool count_only)
    {
        namespace fs = std::filesystem;

        if (!fs::exists("cache"))
            fs::create_directory("cache");

        fs::path cache_file;
        if (options.selectors.empty() && options.properties.empty())
        {
            std::stringstream ss_name;
            ss_name << "cities";
            if (!count_only && options.count > 0)
                ss_name << "_" << options.count;
            if (!count_only)
                ss_name << "_" << (options.order == Order::Descending ? "descending" : "ascending");
            if (options.verbose && !count_only)
                ss_name << "_verbose";
            if (options.strict)
                ss_name << "_strict";
            if (options.pretty)
                ss_name << "_pretty";
            if (count_only)
                ss_name << "_count";
            if (XmlBuilder::can_sign())
                ss_name << "_signed";
            ss_name << ".xml";
            cache_file = fs::path("cache") / ss_name.str();

            if (fs::exists(cache_file))
                return std::make_shared<httpserver::file_response>(cache_file.string(), 200, "application/xml");
        }

        std::map<std::string, std::vector<models::Transaction>> transactions_by_city;

        auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        std::ifstream data("data/transactions.csv");
        std::string line;
        // Skip Header
        std::getline(data, line);

        while (std::getline(data, line))
        {
            auto transaction = parse_transaction(line);

            try
            {
                if (should_skip_transaction(transaction, user_cursor, card_cursor, merchant_cursor, options.strict, options.selectors))
                    continue;
            }
            catch (std::exception& ex)
            {
                return util::make_xml_error(ex.what(), 400);
            }

            if (transaction.merchant_city.empty())
                continue;

            if (!transactions_by_city.count(transaction.merchant_city))
                transactions_by_city.emplace(transaction.merchant_city, std::vector<models::Transaction>{});

            transactions_by_city[transaction.merchant_city].push_back(transaction);
        }

        for (const auto& [city, transactions] : transactions_by_city)
        {
            if (!validate_list_against_properties(transactions, user_cursor, card_cursor, merchant_cursor, options.properties))
                transactions_by_city.erase(city);
        }

        if (!count_only)
        {
            for (auto& pair : transactions_by_city)
            {
                std::sort(pair.second.begin(), pair.second.end(), sort_by_amount{ options.order });
                if (options.count > 0)
                    pair.second.erase(pair.second.begin() + options.count, pair.second.end());
            }
        }
        else
        {
            count_set<std::string> s(transactions_by_city.begin(), transactions_by_city.end(), sort_by_count<std::string>{options.order});
            if (options.count > 0)
            {
                auto iter = s.begin();
                for (int i = 0; i < options.count; ++i)
                    iter++;
                s.erase(iter, s.end());
            }

            XmlBuilder b;
            b
                .add_signature()
                .add_child("Data")
                    .add_child("Counts", {
                            {"order", options.order == Order::Ascending ? "ascending" : "descending"},
                            {"groupedBy", "cities"},
                            {"strict", options.strict ? "true" : "false"}
                    });
            for (const auto& [city, transactions] : s)
                b.add_string("Count", {{"city", city}}, std::to_string(transactions.size()));

            std::string xml = b.serialize(options.pretty);
            if (options.selectors.empty() && options.properties.empty())
            {
                std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
                out.write(xml.c_str(), (std::streamsize)xml.size());
                out.close();
            }
            return std::make_shared<string_response>(xml, 200, "application/xml");
        }

        XmlBuilder::attribute_map attributes {
            {"order", options.order == Order::Descending ? "descending" : "ascending"},
            {"verbose", options.verbose ? "true" : "false"},
            {"strict", options.strict ? "true" : "false"},
            {"groupedBy", "city"}
        };

        if (options.count > 0)
            attributes.emplace("count", std::to_string(options.count));

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_iterator("Results", attributes, transactions_by_city.begin(), transactions_by_city.end(), [&, &verbose = options.verbose](XmlBuilder& b, const auto& pair) {
                    auto& [city, transactions] = pair;
                    b.add_child(city);
                    b.add_array("Transactions", transactions, [&](XmlBuilder& b, const models::Transaction& t) {
                        b.add_child("Transaction", {{"fraud", t.is_fraud ? "true" : "false"}});
                        serialize_amount(b, t.amount);
                        if (verbose)
                            serialize_user_card(b, t.user_id, t.card_id, user_cursor, card_cursor);
                        else
                            serialize_user_card(b, t.user_id, t.card_id);
                        serialize_date(b, t.time);
                        serialize_transaction_type(b, t.type);
                        if (verbose)
                            serialize_merchant(b, t.merchant_id, merchant_cursor);
                        else
                            serialize_merchant(b, t.merchant_id, t.mcc);
                        if (!t.errors.empty())
                        {
                            b.add_array("Errors", t.errors, [](XmlBuilder& b, const auto& s) {
                                b.add_string("Error", s);
                            });
                        }
                        b.step_up();
                    });
                    b.step_up();
                });

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        std::string xml = builder.serialize(options.pretty);

        if (options.selectors.empty() && options.properties.empty())
        {
            std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
            out.write(xml.c_str(), (std::streamsize)xml.size());
            out.close();
        }
        return std::make_shared<string_response>(xml, 200, "application/xml");
    }

    const Ref<http_response> process_months(TransactionQueryOptions& options, lmdb::env& env, bool count_only)
    {
        namespace fs = std::filesystem;

        constexpr const char* months[12] {
            "January",
            "February",
            "March",
            "April",
            "May",
            "June",
            "July",
            "August",
            "September",
            "October",
            "November",
            "December"
        };

        if (!fs::exists("cache"))
            fs::create_directory("cache");

        fs::path cache_file;
        if (options.selectors.empty() && options.properties.empty())
        {
            std::stringstream ss_name;
            ss_name << "months";
            if (!count_only && options.count > 0)
                ss_name << "_" << options.count;
            if (!count_only)
                ss_name << "_" << (options.order == Order::Descending ? "descending" : "ascending");
            if (options.verbose && !count_only)
                ss_name << "_verbose";
            if (options.strict)
                ss_name << "_strict";
            if (options.pretty)
                ss_name << "_pretty";
            if (count_only)
                ss_name << "_count";
            if (XmlBuilder::can_sign())
                ss_name << "_signed";
            ss_name << ".xml";
            cache_file = fs::path("cache") / ss_name.str();

            if (fs::exists(cache_file))
                return std::make_shared<httpserver::file_response>(cache_file.string(), 200, "application/xml");
        }

        std::map<int, std::vector<models::Transaction>> transactions_by_month;

        auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        std::ifstream data("data/transactions.csv");
        std::string line;
        // Skip Header
        std::getline(data, line);

        while (std::getline(data, line))
        {
            auto transaction = parse_transaction(line);

            try
            {
                if (should_skip_transaction(transaction, user_cursor, card_cursor, merchant_cursor, options.strict, options.selectors))
                    continue;
            }
            catch (std::exception& ex)
            {
                return util::make_xml_error(ex.what(), 400);
            }

            struct tm* ptm = gmtime(&transaction.time);
            auto month = ptm->tm_mon;

            if (!transactions_by_month.count(month))
                transactions_by_month.emplace(month, std::vector<models::Transaction>{});

            transactions_by_month[month].push_back(transaction);
        }

        if (!count_only)
        {
            for (auto& pair : transactions_by_month)
            {
                std::sort(pair.second.begin(), pair.second.end(), sort_by_amount{ options.order });
                if (options.count > 0)
                    pair.second.erase(pair.second.begin() + options.count, pair.second.end());
            }
        }
        else
        {
            count_set<int> s(transactions_by_month.begin(),
                    transactions_by_month.end(), sort_by_count<int>{options.order});
            if (options.count > 0)
            {
                auto iter = s.begin();
                for (int i = 0; i < options.count; ++i)
                    iter++;
                s.erase(iter, s.end());
            }

            XmlBuilder b;
            b
                    .add_signature()
                    .add_child("Data")
                        .add_child("Counts", {
                            {"order", options.order == Order::Ascending ? "ascending" : "descending"},
                            {"groupedBy", "months"},
                            {"strict", options.strict ? "true" : "false"}
                        });
            for (const auto& [month, transactions] : s)
                b.add_string("Count", {{"month", months[month]}}, std::to_string(transactions.size()));

            std::string xml = b.serialize(options.pretty);
            if (options.selectors.empty() && options.properties.empty())
            {
                std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
                out.write(xml.c_str(), (std::streamsize)xml.size());
                out.close();
            }
            return std::make_shared<string_response>(xml, 200, "application/xml");
        }

        XmlBuilder::attribute_map attributes {
                {"order", options.order == Order::Descending ? "descending" : "ascending"},
                {"verbose", options.verbose ? "true" : "false"},
                {"strict", options.strict ? "true" : "false"},
                {"groupedBy", "month"}
        };
        if (options.count > 0)
            attributes.emplace("count", std::to_string(options.count));

        XmlBuilder builder;
        builder
                .add_signature()
                .add_child("Data")
                .add_iterator("Results", attributes, transactions_by_month.begin(), transactions_by_month.end(), [&, &verbose = options.verbose](XmlBuilder& b, const auto& pair) {
                    auto& [month, transactions] = pair;
                    b.add_child(months[month]);
                    b.add_array("Transactions", transactions, [&](XmlBuilder& b, const models::Transaction& t) {
                        b.add_child("Transaction", {{"fraud", t.is_fraud ? "true" : "false"}});
                        serialize_amount(b, t.amount);
                        if (verbose)
                            serialize_user_card(b, t.user_id, t.card_id, user_cursor, card_cursor);
                        else
                            serialize_user_card(b, t.user_id, t.card_id);
                        serialize_date(b, t.time);
                        serialize_transaction_type(b, t.type);
                        if (verbose)
                            serialize_merchant(b, t.merchant_id, merchant_cursor);
                        else
                            serialize_merchant(b, t.merchant_id, t.mcc);
                        if (!t.errors.empty())
                        {
                            b.add_array("Errors", t.errors, [](XmlBuilder& b, const auto& s) {
                                b.add_string("Error", s);
                            });
                        }
                        b.step_up();
                    });
                    b.step_up();
                });

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        std::string xml = builder.serialize(options.pretty);

        if (options.selectors.empty() && options.properties.empty())
        {
            std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
            out.write(xml.c_str(), (std::streamsize)xml.size());
            out.close();
        }
        return std::make_shared<string_response>(xml, 200, "application/xml");
    }

    const Ref<http_response> process_states(TransactionQueryOptions& options, lmdb::env& env, bool count_only)
    {
        namespace fs = std::filesystem;

        if (!fs::exists("cache"))
            fs::create_directory("cache");

        fs::path cache_file;
        if (options.selectors.empty() && options.properties.empty())
        {
            std::stringstream ss_name;
            ss_name << "states";
            if (!count_only && options.count > 0)
                ss_name << "_" << options.count;
            if (!count_only)
                ss_name << "_" << (options.order == Order::Descending ? "descending" : "ascending");
            if (options.verbose && !count_only)
                ss_name << "_verbose";
            if (options.strict)
                ss_name << "_strict";
            if (options.pretty)
                ss_name << "_pretty";
            if (count_only)
                ss_name << "_count";
            if (XmlBuilder::can_sign())
                ss_name << "_signed";
            ss_name << ".xml";
            cache_file = fs::path("cache") / ss_name.str();

            if (fs::exists(cache_file))
                return std::make_shared<httpserver::file_response>(cache_file.string(), 200, "application/xml");
        }

        std::map<std::string, std::vector<models::Transaction>> transactions_by_state;

        auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        std::ifstream data("data/transactions.csv");
        std::string line;
        // Skip Header
        std::getline(data, line);

        while (std::getline(data, line))
        {
            auto transaction = parse_transaction(line);

            try
            {
                if (should_skip_transaction(transaction, user_cursor, card_cursor, merchant_cursor, options.strict, options.selectors))
                    continue;
            }
            catch (std::exception& ex)
            {
                return util::make_xml_error(ex.what(), 400);
            }

            if (transaction.merchant_state.empty())
                continue;

            // Not a state abbreviation, must be a country
            if (transaction.merchant_state.size() > 2)
                continue;

            if (!transactions_by_state.count(transaction.merchant_state))
                transactions_by_state.emplace(transaction.merchant_state, std::vector<models::Transaction>{});

            transactions_by_state[transaction.merchant_state].push_back(transaction);
        }

        for (const auto& [state, transactions] : transactions_by_state)
        {
            if (!validate_list_against_properties(transactions, user_cursor, card_cursor, merchant_cursor, options.properties))
                transactions_by_state.erase(state);
        }

        if (!count_only)
        {
            for (auto& pair : transactions_by_state)
            {
                std::sort(pair.second.begin(), pair.second.end(), sort_by_amount{ options.order });
                if (options.count > 0)
                    pair.second.erase(pair.second.begin() + options.count, pair.second.end());
            }
        }
        else
        {
            count_set<std::string> s(transactions_by_state.begin(),
                    transactions_by_state.end(), sort_by_count<std::string>{options.order});
            if (options.count > 0)
            {
                auto iter = s.begin();
                for (int i = 0; i < options.count; ++i)
                    iter++;
                s.erase(iter, s.end());
            }

            XmlBuilder b;
            b
                .add_signature()
                .add_child("Data")
                    .add_child("Counts", {
                        {"order", options.order == Order::Ascending ? "ascending" : "descending"},
                        {"groupedBy", "states"},
                        {"strict", options.strict ? "true" : "false"}
                    });
            for (const auto& [state, transactions] : s)
                b.add_string("Count", {{"state", state}}, std::to_string(transactions.size()));

            std::string xml = b.serialize(options.pretty);
            if (options.selectors.empty() && options.properties.empty())
            {
                std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
                out.write(xml.c_str(), (std::streamsize)xml.size());
                out.close();
            }
            return std::make_shared<string_response>(xml, 200, "application/xml");
        }

        XmlBuilder::attribute_map attributes {
                {"order", options.order == Order::Descending ? "descending" : "ascending"},
                {"verbose", options.verbose ? "true" : "false"},
                {"strict", options.strict ? "true" : "false"},
                {"groupedBy", "state"}
        };
        if (options.count > 0)
            attributes.emplace("count", std::to_string(options.count));

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_iterator("Results", attributes, transactions_by_state.begin(), transactions_by_state.end(), [&, &verbose = options.verbose](XmlBuilder& b, const auto& pair) {
                    auto& [state, transactions] = pair;
                    b.add_child(state);
                    b.add_array("Transactions", transactions, [&](XmlBuilder& b, const models::Transaction& t) {
                        b.add_child("Transaction", {{"fraud", t.is_fraud ? "true" : "false"}});
                        serialize_amount(b, t.amount);
                        if (verbose)
                            serialize_user_card(b, t.user_id, t.card_id, user_cursor, card_cursor);
                        else
                            serialize_user_card(b, t.user_id, t.card_id);
                        serialize_date(b, t.time);
                        serialize_transaction_type(b, t.type);
                        if (verbose)
                            serialize_merchant(b, t.merchant_id, merchant_cursor);
                        else
                            serialize_merchant(b, t.merchant_id, t.mcc);
                        if (!t.errors.empty())
                        {
                            b.add_array("Errors", t.errors, [](XmlBuilder& b, const auto& s) {
                                b.add_string("Error", s);
                            });
                        }
                        b.step_up();
                    });
                    b.step_up();
                });

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        std::string xml = builder.serialize(options.pretty);

        if (options.selectors.empty() && options.properties.empty())
        {
            std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
            out.write(xml.c_str(), (std::streamsize)xml.size());
            out.close();
        }
        return std::make_shared<string_response>(xml, 200, "application/xml");
    }

    const Ref<http_response> process_transactions(TransactionQueryOptions& options, lmdb::env& env, bool count_only)
    {
        namespace fs = std::filesystem;

        if (!fs::exists("cache"))
            fs::create_directory("cache");

        fs::path cache_file;
        if (options.selectors.empty())
        {
            std::stringstream ss_name;
            ss_name << "transactions";
            if (!count_only && options.count > 0)
                ss_name << "_" << options.count;
            if (!count_only)
                ss_name << "_" << (options.order == Order::Descending ? "descending" : "ascending");
            if (options.verbose && !count_only)
                ss_name << "_verbose";
            if (options.strict)
                ss_name << "_strict";
            if (options.pretty)
                ss_name << "_pretty";
            if (count_only)
                ss_name << "_count";
            if (XmlBuilder::can_sign())
                ss_name << "_signed";
            // TODO(Jordan): Encode selectors into cache name
            ss_name << ".xml";
            cache_file = fs::path("cache") / ss_name.str();

            // TODO(Jordan): Check if timestamp on file is newer than transactions.csv
            if (fs::exists(cache_file))
                return std::make_shared<httpserver::file_response>(cache_file.string(), 200, "application/xml");
        }

        std::vector<models::Transaction> transactions;

        auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
        auto user_dbi = lmdb::dbi::open(rtxn, "users");
        auto card_dbi = lmdb::dbi::open(rtxn, "cards");
        auto merchant_dbi = lmdb::dbi::open(rtxn, "merchants");
        auto user_cursor = lmdb::cursor::open(rtxn, user_dbi);
        auto card_cursor = lmdb::cursor::open(rtxn, card_dbi);
        auto merchant_cursor = lmdb::cursor::open(rtxn, merchant_dbi);

        std::ifstream data("data/transactions.csv");
        std::string line;
        // Skip Header
        std::getline(data, line);

        while (std::getline(data, line))
        {
            auto transaction = parse_transaction(line);

            try
            {
                if (should_skip_transaction(transaction, user_cursor, card_cursor, merchant_cursor, options.strict, options.selectors))
                    continue;
            }
            catch (std::exception& ex)
            {
                return util::make_xml_error(ex.what(), 400);
            }

            transactions.push_back(transaction);
        }

        if (!count_only) std::sort(transactions.begin(), transactions.end(), sort_by_amount{options.order});
        if (options.count > 0) transactions.erase(transactions.begin() + options.count, transactions.end());

        if (count_only)
        {
            XmlBuilder b;
            b
                .add_signature()
                .add_child("Data")
                    .add_string("Count", {{"strict", options.strict ? "true" : "false"}}, std::to_string(transactions.size()));

            std::string xml = b.serialize(options.pretty);

            if (options.selectors.empty())
            {
                std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
                out.write(xml.c_str(), (std::streamsize)xml.size());
                out.close();
            }
            return std::make_shared<string_response>(xml, 200, "application/xml");
        }

        std::unordered_map<std::string, std::string> attributes {
                {"order", options.order == Order::Descending ? "descending" : "ascending"},
                {"verbose", options.verbose ? "true" : "false"},
                {"strict", options.strict ? "true" : "false"}
        };
        if (options.count > 0)
            attributes.emplace("count", std::to_string(options.count));

        XmlBuilder builder;
        builder
            .add_signature()
            .add_child("Data")
                .add_iterator("Transactions", attributes, transactions.begin(), transactions.end(), [&, &verbose = options.verbose](XmlBuilder& b, const models::Transaction& t) {
                    b.add_child("Transaction", {{"fraud", t.is_fraud ? "true" : "false"}});
                    serialize_amount(b, t.amount);
                    if (verbose)
                        serialize_user_card(b, t.user_id, t.card_id, user_cursor, card_cursor);
                    else
                        serialize_user_card(b, t.user_id, t.card_id);
                    serialize_date(b, t.time);
                    serialize_transaction_type(b, t.type);
                    if (verbose)
                        serialize_merchant(b, t.merchant_id, merchant_cursor);
                    else
                        serialize_merchant(b, t.merchant_id, t.mcc);
                    serialize_location(b, t.merchant_city, t.merchant_state, t.zip);
                    if (!t.errors.empty())
                    {
                        b.add_array("Errors", t.errors, [](XmlBuilder& b, const auto& s) {
                            b.add_string("Error", s);
                        });
                    }
                    b.step_up();
                });

        user_cursor.close();
        card_cursor.close();
        merchant_cursor.close();
        rtxn.abort();

        std::string xml = builder.serialize(options.pretty);

        if (options.selectors.empty())
        {
            std::ofstream out(cache_file, std::ios::out | std::ios::trunc);
            out.write(xml.c_str(), (std::streamsize)xml.size());
            out.close();
        }
        return std::make_shared<string_response>(xml, 200, "application/xml");
    }

    const Ref<http_response> get_models(TransactionQueryOptions& options)
    {
        namespace fs = std::filesystem;

        if (!fs::exists("cache"))
            fs::create_directory("cache");

        std::string name = "cache/models";
        if (options.pretty)
            name += "_pretty";
        name += ".xml";

        if (fs::exists(name))
            return std::make_shared<httpserver::file_response>(name, 200, "application/xml");

        XmlBuilder b;
        b
            .add_signature()
            .add_array("Models", {
                "transactions",
                "users",
                "cards",
                "merchants",
                "cities",
                "states",
                "zipcodes"
            }, [](XmlBuilder& b, const std::string_view& model) { b.add_string("Model", model); });

        auto serialized = b.serialize(options.pretty);
        std::ofstream cache_file(name, std::ios::out | std::ios::trunc);
        cache_file.write(serialized.c_str(), (std::streamsize)serialized.size());
        cache_file.close();
        return std::make_shared<string_response>(serialized, 200, "application/xml");
    }

    const Ref<http_response> queries::process(const http_request& req) try
    {
        if (req.get_path_pieces().size() > 3)
            return util::make_xml_error("Queries must be done in the following format: /query/{model}[/count]!", 400);

        bool count_only = false;
        if (req.get_path_pieces().size() == 3 && req.get_path_piece(2) != "count")
            return util::make_xml_error("Queries must be done in the following format: /query/{model}[/count]!", 400);
        else if (req.get_path_pieces().size() == 3)
            count_only = true;

        auto query_type = req.get_path_piece(1);
        std::transform(query_type.begin(), query_type.end(), query_type.begin(), ::tolower);

        if (query_type == "schema" || query_type == "schema.json")
            return std::make_shared<httpserver::file_response>("data/schema.json", 200, "application/json");

        auto args = req.get_args();
        TransactionQueryOptions options;
        std::string sort;

        auto type = req.get_header("Content-Type");
        if (type == "application/json")
        {
            nlohmann::json j;
            try
            {
                j = nlohmann::json::parse(req.get_content());
                p_validator.validate(j);
            }
            catch (nlohmann::json::parse_error& ex)
            {
                std::string err = ex.what();
                auto idx = err.find(' ');
                err.erase(0, idx + 1);
                return util::make_xml_error("An error occurred while parsing json content: "s + err, 400);
            }
            catch (std::exception& ex)
            {
                return util::make_xml_error("Failed to validate json against /query/schema.json: "s + ex.what(), 400);
            }

            if (j.contains("count"))
                options.count = j["count"].get<int>();
            if (j.contains("order"))
                options.order = j["order"].get<std::string>() == "ascending" ? Order::Ascending : Order::Descending;
            if (j.contains("verbose"))
                options.verbose = j["verbose"].get<bool>();
            if (j.contains("strict"))
                options.strict = j["strict"].get<bool>();
            if (j.contains("pretty"))
                options.pretty = j["pretty"].get<bool>();
            if (j.contains("selectors"))
            {
                auto selector_arr = j["selectors"].get<nlohmann::json::array_t>();
                for (size_t i = 0; i < selector_arr.size(); ++i)
                {
                    auto& selector = selector_arr[i];

                    auto field = transaction_field_from_json(selector["field"]);
                    auto selectorType = selector_type_from_string(selector["type"].get<std::string>());
                    std::vector<std::string> values;
                    if (selector["value"].is_array())
                    {
                        for (const auto& value : selector["value"])
                            values.emplace_back(value.get<std::string>());
                    }
                    else if (selector["value"].is_object())
                    {
                        // TODO(Jordan): Implement
                    }
                    else
                    {
                        values.emplace_back(selector["value"].get<std::string>());
                    }
                    auto qs = QuerySelector { field, selectorType, values };
                    auto [valid, error] { validate_selector(qs) };
                    if (!valid)
                        return util::make_xml_error("An error occurred while validating selectors["s + std::to_string(i) + "]: "s + error, 400);
                    options.selectors.push_back(qs);
                }
            }
            if (j.contains("properties"))
            {
                auto property_arr = j["properties"].get<nlohmann::json::array_t>();
                for (size_t i = 0; i < property_arr.size(); ++i)
                {
                    auto& property = property_arr[i];

                    auto field = transaction_field_from_json(property["field"]);
                    auto condition = property_condition_from_string(property["condition"].get<std::string>());
                    auto selectorType = selector_type_from_string(property["type"].get<std::string>());
                    std::vector<std::string> values;
                    if (property["value"].is_array())
                    {
                        for (const auto& value : property["value"])
                            values.emplace_back(value.get<std::string>());
                    }
                    else if (property["value"].is_object())
                    {
                        // TODO(Jordan): Implement
                    }
                    else
                    {
                        values.emplace_back(property["value"].get<std::string>());
                    }
                    auto qp = QueryProperty { condition, { field, selectorType, values } };
                    auto [valid, error] { validate_selector(qp.selector) };
                    if (!valid)
                        return util::make_xml_error("An error occurred while validating properties["s + std::to_string(i) + "]: "s + error, 400);
                    options.properties.push_back(qp);
                }
            }
        }
        else
        {
            if (args.find("count") != args.end())
            {
                auto [ec, value] { util::parse<int>(req.get_arg("count")) };
                if (ec == std::errc::invalid_argument)
                    return util::make_xml_error("\"count\" must be an integer!", 400);
                else if (ec == std::errc::result_out_of_range)
                    return util::make_xml_error("\"count\" is too large!", 400);
                else if (value <= 0)
                    return util::make_xml_error("\"count\" must be greater than 0!", 400);
                options.count = value;
            }

            if (args.find("order") != args.end())
            {
                std::string temp = req.get_arg("order");
                std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
                if (temp == "descending" || temp == "desc" || temp == "dc")
                    options.order = Order::Descending;
                else if (temp == "ascending" || temp == "asc" || temp == "ac")
                    options.order = Order::Ascending;
                else
                    return util::make_xml_error("\"order\" must be either ascending or descending!", 400);
            }

            if (args.find("verbose") != args.end())
            {
                std::string temp = req.get_arg("verbose");
                std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
                if (temp == "true" || temp == "1" || temp == "on" || temp == "yes" || temp == "y")
                    options.verbose = true;
                else if (temp == "false" || temp == "0" || temp == "off" || temp == "no" || temp == "n")
                    options.verbose = false;
                else return util::make_xml_error("\"verbose\" must be a boolean value!", 400);
            }

            if (args.find("strict") != args.end())
            {
                std::string temp = req.get_arg("strict");
                std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
                if (temp == "true" || temp == "1" || temp == "on" || temp == "yes" || temp == "y")
                    options.strict = true;
                else if (temp == "false" || temp == "0" || temp == "off" || temp == "no" || temp == "n")
                    options.strict = false;
                else return util::make_xml_error("\"strict\" must be a boolean value!", 400);
            }

            if (args.find("pretty") != args.end())
            {
                std::string temp = req.get_arg("pretty");
                std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
                if (temp == "true" || temp == "1" || temp == "on" || temp == "yes" || temp == "y")
                    options.pretty = true;
                else if (temp == "false" || temp == "0" || temp == "off" || temp == "no" || temp == "n")
                    options.pretty = false;
                else return util::make_xml_error("\"pretty\" must be a boolean value!", 400);
            }
        }

        if (query_type == "transaction" || query_type == "transactions")
            return process_transactions(options, *p_env, count_only);
        else if (query_type == "model" || query_type == "models")
            return get_models(options);
        else if (query_type == "state" || query_type == "states")
            return process_states(options, *p_env, count_only);
        else if (query_type == "month" || query_type == "months")
            return process_months(options, *p_env, count_only);
        else if (query_type == "city" || query_type == "cities")
            return process_cities(options, *p_env, count_only);

        return util::make_xml_error("Not yet implemented!", 500);
    }
    catch (std::exception& e)
    {
        spdlog::error("An error occurred while processing a transaction query: {}", e.what());
        return util::make_xml_error("An error occurred while processing your request, please notify a webadmin.", 500);
    }
}