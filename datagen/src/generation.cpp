#include "generation.hpp"

#include "faker.hpp"

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
