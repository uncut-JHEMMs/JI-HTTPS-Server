#pragma once

#include <string_view>
#include <cstdlib>
#include <cctype>

#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOMDocument.hpp>

namespace util
{
    static inline bool is_integer(const std::string_view& str)
    {
        if (str.empty() || ((!isdigit(str[0])) && (str[0] != '-') && (str[0] != '+'))) return false;

        char* p;
        strtol(str.data(), &p, 10);

        return (*p == 0);
    }

    std::string sign_document(xercesc::DOMDocument* doc);

    class XStr
    {
        XMLCh* p_unicode;
    public:
        explicit XStr(const char* const str) : p_unicode(xercesc::XMLString::transcode(str)) {}
        ~XStr() { xercesc::XMLString::release(&p_unicode); }

        inline const XMLCh* unicode() const { return p_unicode; }
        inline operator XMLCh*() const { return p_unicode; }
    };

}

#define X(str) util::XStr(str)
