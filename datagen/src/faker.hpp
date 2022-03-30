#pragma once

#include <string>

/**
 * This is a simple macro to reduce repeated code, it makes an inline static method with the name
 * passed in to the `method` argument. This method will call the python method given with `py`, then
 * return whatever was returned during the method call.
 */
#define EXPOSE_METHOD(method, py) inline static std::string method() { return Faker::CallMethod(#py); }

/**
 * Forward declaration of PyObject, just so code that uses Faker don't have to include the
 * massive Python.h header file when they aren't actually going to use it.
 */
struct _object;
typedef struct _object PyObject;

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
        PyObject* module;

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
    };

    EXPOSE_METHOD(FirstName, first_name)
    EXPOSE_METHOD(LastName, last_name)
}
