//
// Created by Shad Shadster on 4/21/2022.
//

#ifndef FAKER_CPP_COMPANY_HPP
#define FAKER_CPP_COMPANY_HPP

#include "locales.hpp"
#include "helpers.hpp"
#include "name.hpp"

#define LOCALE(_locale_, elem) faker::locales::_locale_::elem, faker::locales::_locale_::elem##_LEN

namespace faker::company
{
    static inline const std::string_view& suffix(faker::locales::locale locale = faker::locales::ENGLISH)
    {
        switch (locale)
        {
        case locales::ENGLISH:
            return faker::helpers::array_element(LOCALE(en, COMPANY_SUFFIX));
        }
    }

    static inline std::string name(faker::locales::locale locale = faker::locales::ENGLISH)
    {
        auto i = helpers::number_in_range(0, 1);

        if (i == 0)
        {
            std::string result{faker::name::last_name(locale)};
            result.append(" ");
            result.append(faker::company::suffix(locale));
            return result;
        }
        else
        {
            std::string result{faker::name::last_name(locale)};
            result.append(" and ");
            result.append(faker::name::last_name(locale));
            result.append( " ");
            result.append(faker::company::suffix(locale));
            return result;
        }
    }
}

#undef LOCALE

#endif //FAKER_CPP_COMPANY_HPP
