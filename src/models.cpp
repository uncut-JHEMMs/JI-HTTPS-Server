#include "models.hpp"

std::string_view models::transaction_type_to_string(TransactionType type)
{
    switch (type)
    {
    case TransactionType::Chip: return "Chip Transaction";
    case TransactionType::Online: return "Online Transaction";
    case TransactionType::Swipe: return "Swipe Transaction";
    default: return "unknown Transaction";
    }
}
