#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <ctime>

namespace models
{
    enum class TransactionType
    {
        Chip,
        Online,
        Swipe,
        Unknown
    };

    struct Transaction
    {
        uint16_t user_id;
        uint8_t card_id;
        time_t time;
        long amount;
        TransactionType type;
        int64_t merchant_id;
        std::string merchant_city;
        std::string merchant_state;
        uint32_t zip;
        uint32_t mcc;
        std::vector<std::string> errors;
        bool is_fraud;
    };

    enum class MerchantCategory : uint8_t
    {
        Agricultural,
        Contracted,
        TravelAndEntertainment,
        CarRental,
        Lodging,
        Transportation,
        Utility,
        RetailOutlet,
        ClothingStore,
        MiscStore,
        Business,
        ProfessionalOrMembership,
        Government
    };

    std::string_view transaction_type_to_string(TransactionType type);
}