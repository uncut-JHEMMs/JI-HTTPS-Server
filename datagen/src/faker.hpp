#pragma once

#include <string>
#include <string_view>
#include "model.hpp"

/**
 * This is a simple macro to reduce repeated code, it makes an inline static method with the name
 * passed in to the `method` argument. This method will call the python method given with `py`, then
 * return whatever was returned during the method call.
 */
#define EXPOSE_METHOD(method, py) inline static std::string method() { return Faker::CallMethod(#py); }
#define EXPOSE_CARD_METHOD(method, py) inline static std::string method(Card::CardType type = Card::None) { return Faker::CallCardMethod(#py, type); }

/**
 * Forward declaration of some Python structs, just so code that uses Faker don't have to
 * include the massive Python.h header file when they aren't actually going to use it.
 */
struct _object;
struct _ts;
typedef struct _object PyObject;
typedef struct _ts PyThreadState;

/**
 * This is essentially a map of the Faker python library.
 *
 * I expose methods from that library to C++
 */
namespace faker
{
    /**
     * I could probably make this completely private by defining it in the source
     * file. Especially since I only want to use exposed methods. Unfortunately,
     * I won't be able to inline the method calls if I do that.
     */
    class Faker
    {
        static Faker Instance;

        PyObject* module;
        PyThreadState* ts;

        /**
         * Making this private, but not deleted, so I can statically initialize this
         * class once, and deconstruct the class when the program exits.
         */
        Faker();
        ~Faker() noexcept;
    public:

        /**
         * Calls a method from the faker.Faker Python class
         *
         * @param view The method to call
         * @return The result of the method, as a string
         */
        static std::string CallMethod(const std::string_view& view);
        static std::string CallCardMethod(const std::string_view& view, Card::CardType type);
    };

    EXPOSE_METHOD(FirstName, first_name)
    EXPOSE_METHOD(LastName, last_name)

    namespace credit_card
    {
        EXPOSE_METHOD(ExpirationDate, credit_card_expire)
        EXPOSE_CARD_METHOD(CardNumber, credit_card_number)
        EXPOSE_CARD_METHOD(SecurityCode, credit_card_security_code)
    }
}
