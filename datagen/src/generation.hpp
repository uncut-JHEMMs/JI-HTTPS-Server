#pragma once

#include "model.hpp"

namespace generation
{
    User generate_user();
    Card generate_card();
    Merchant generate_merchant(const std::string_view& mcc);
}
