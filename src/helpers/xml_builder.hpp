#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <libxml/tree.h>

class XmlBuilder
{
    bool p_add_signature;
    xmlDocPtr p_doc;
    xmlNodePtr p_cur_node;

    static std::string s_certificate;
    static std::string s_private_key;
    static bool s_can_sign;
public:
    using attribute_map = std::unordered_map<std::string, std::string>;

    XmlBuilder();
    ~XmlBuilder();

    XmlBuilder& add_string(const std::string_view& name, const std::string_view& data);
    XmlBuilder& add_string(const std::string_view& name, const attribute_map& attributes, const std::string_view& data);

    XmlBuilder& add_child(const std::string_view& name);
    XmlBuilder& add_child(const std::string_view& name, const attribute_map& attributes);

    XmlBuilder& add_signature();

    XmlBuilder& step_up();

    std::string serialize();

    template<typename T, typename Func>
    inline XmlBuilder& add_array(const std::string_view& name, const attribute_map& attributes, const std::vector<T>& elems, Func for_each)
    {
        add_child(name, attributes);
        for (const auto& elem : elems)
            for_each(*this, elem);
        return step_up();
    }

    template<typename T, typename Func>
    inline XmlBuilder& add_array(const std::string_view& name, const std::vector<T>& elems, Func for_each)
    {
        return add_array(name, attribute_map{}, elems, for_each);
    }

    template<typename T, typename Func>
    inline XmlBuilder& add_array(const std::string_view& name, const std::initializer_list<T>& elems, Func for_each)
    {
        return add_array(name, std::vector<T>{elems}, for_each);
    }

    template<typename T, typename Func>
    inline XmlBuilder& add_array(const std::string_view& name, const attribute_map& attributes, const std::initializer_list<T>& elems, Func for_each)
    {
        return add_array(name, attributes, std::vector<T>{elems}, for_each);
    }

    inline static void initialize_signing(std::string certificate, std::string private_key)
    {
        XmlBuilder::s_certificate = std::move(certificate);
        XmlBuilder::s_private_key = std::move(private_key);
        XmlBuilder::s_can_sign = true;
    }

private:
    void serialize_signature();
};