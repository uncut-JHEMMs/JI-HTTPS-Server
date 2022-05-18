//
// Created by Shad Shadster on 4/19/2022.
//

#ifndef FAKER_CPP_NAME_HPP
#define FAKER_CPP_NAME_HPP

#include "locales.hpp"
#include "helpers.hpp"

#define LOCALE(_locale_, elem) faker::locales::_locale_::elem, faker::locales::_locale_::elem##_LEN

namespace faker::name
{
    static inline const std::string_view& first_name(faker::locales::locale locale = faker::locales::ENGLISH)
    {
        switch (locale)
        {
        case locales::ENGLISH:
            return faker::helpers::array_element(LOCALE(en, NAME_FIRST_NAME));
        }
    }

    static inline const std::string_view& last_name(faker::locales::locale locale = faker::locales::ENGLISH)
    {
        switch (locale)
        {
        case locales::ENGLISH:
            return faker::helpers::array_element(LOCALE(en, NAME_LAST_NAME));
        }
    }
}

#undef LOCALE

#endif //FAKER_CPP_NAME_HPP
