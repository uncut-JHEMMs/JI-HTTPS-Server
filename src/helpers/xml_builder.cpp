#include "xml_builder.hpp"

#include "utilities.hpp"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#define X (xmlChar*)

bool XmlBuilder::s_can_sign = false;
std::string XmlBuilder::s_private_key = std::string();
std::string XmlBuilder::s_certificate = std::string();

XmlBuilder::XmlBuilder() : p_add_signature(false)
{
    this->p_doc = xmlNewDoc(X"1.0");
    this->p_cur_node = xmlNewNode(nullptr, X"Envelope");
    xmlNewProp(p_cur_node, X"xmlns", X"urn:envelope");
    xmlDocSetRootElement(p_doc, p_cur_node);
}

XmlBuilder::~XmlBuilder()
{
    xmlFreeDoc(p_doc);
    xmlCleanupParser();
}

XmlBuilder& XmlBuilder::add_string(const std::string_view& name, const std::string_view& data)
{
    return add_string(name, attribute_map{}, data);
}

XmlBuilder& XmlBuilder::add_string(const std::string_view& name, const attribute_map& attributes, const std::string_view& data)
{
    auto node = xmlNewChild(p_cur_node, nullptr, X name.data(), X data.data());
    for (const auto& [attribute, value] : attributes)
    {
        xmlNewProp(node, X attribute.c_str(), X value.c_str());
    }
    return *this;
}

XmlBuilder& XmlBuilder::add_child(const std::string_view& name)
{
    return add_child(name, attribute_map{});
}

XmlBuilder& XmlBuilder::add_child(const std::string_view& name, const attribute_map& attributes)
{
    this->p_cur_node = xmlNewChild(p_cur_node, nullptr, X name.data(), nullptr);
    for (const auto& [attribute, value] : attributes)
    {
        xmlNewProp(p_cur_node, X attribute.c_str(), X value.c_str());
    }
    return *this;
}

XmlBuilder& XmlBuilder::add_empty(const std::string_view& name)
{
    return add_empty(name, attribute_map{});
}

XmlBuilder& XmlBuilder::add_empty(const std::string_view& name, const attribute_map& attributes)
{
    return add_child(name, attributes).step_up();
}

XmlBuilder& XmlBuilder::add_signature()
{
    if (s_can_sign)
        this->p_add_signature = true;
    return *this;
}

XmlBuilder& XmlBuilder::step_up()
{
    this->p_cur_node = p_cur_node->parent;
    return *this;
}

std::string XmlBuilder::serialize(bool pretty)
{
    auto old = xmlKeepBlanksDefault(pretty ? 0 : 1);
    if (p_add_signature)
        serialize_signature(pretty);

    xmlChar* str;
    int size;
    xmlDocDumpFormatMemoryEnc(p_doc, &str, &size, "UTF-8", pretty ? 1 : 0);
    xmlKeepBlanksDefault(old);

    std::string value((const char*)str, (size_t)size);
    xmlFree(str);

    return value;
}

using bytes = std::vector<uint8_t>;

bytes compute_hash(const uint8_t* data, size_t size)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
    EVP_DigestUpdate(ctx, data, size);

    bytes hash;
    hash.resize(SHA_DIGEST_LENGTH);
    uint hash_len;
    EVP_DigestFinal_ex(ctx, hash.data(), &hash_len);
    EVP_MD_CTX_free(ctx);
    return hash;
}

void XmlBuilder::serialize_signature(bool pretty)
{
    xmlChar* str;
    int size;
    auto old = xmlKeepBlanksDefault(pretty ? 1 : 0);
    xmlDocDumpFormatMemoryEnc(p_doc, &str, &size, "UTF-8", pretty ? 1 : 0);
    xmlKeepBlanksDefault(old);

    bytes hash = compute_hash(str, (size_t)size);

    size_t sig_len;
    bytes signature;

    BIO* bio = BIO_new(BIO_s_mem());
    BIO_write(bio, s_private_key.c_str(), s_private_key.size());

    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    EVP_DigestSignInit(ctx, nullptr, EVP_sha1(), nullptr, key);
    EVP_DigestSignUpdate(ctx, str, size);

    EVP_DigestSignFinal(ctx, nullptr, &sig_len);
    signature.resize(sig_len);
    EVP_DigestSignFinal(ctx, signature.data(), &sig_len);

    xmlFree(str);

    std::string hash_b64 = util::base64_encode(hash.data(), hash.size());
    std::string sig_b64 = util::base64_encode(signature.data(), signature.size());

    std::string cert_b64 = util::base64_encode((const uint8_t*)s_certificate.c_str(), s_certificate.size());

    auto sig_node = xmlNewChild(xmlDocGetRootElement(p_doc), nullptr, X "Signature", nullptr);
    xmlNewProp(sig_node, X "xmlns", X "http://www.w3.org/2000/09/xmldsig#");

    auto sig_info = xmlNewChild(sig_node, nullptr, X "SignedInfo", nullptr);
    xmlNewProp(
            xmlNewChild(sig_info, nullptr, X "CanonicalizationMethod", nullptr),
            X"Algorithm",
            X"http://www.w3.org/TR/2001/REC-xml-c14n-20010315#WithComments");
    xmlNewProp(
            xmlNewChild(sig_info, nullptr, X"SignatureMethod", nullptr),
            X"Algorithm",
            X"http://www.w3.org/2000/09/xmldsig#dsa-sha1"
            );

    auto ref_node = xmlNewChild(sig_info, nullptr, X"Reference", nullptr);
    xmlNewProp(ref_node, X"URI", X"");

    xmlNewProp(
            xmlNewChild(
                    xmlNewChild(ref_node, nullptr, X"Transforms", nullptr),
                    nullptr,
                    X"Transform",
                    nullptr
                    ),
            X"Algorithm",
            X"http://www.w3.org/2000/09/xmldsig#enveloped-signature"
            );
    xmlNewProp(xmlNewChild(ref_node, nullptr, X"DigestMethod", nullptr), X"Algorithm", X"http://www.w3.org/2000/09/xmldsig#sha1");
    xmlNewChild(ref_node, nullptr, X"DigestValue", X hash_b64.c_str());
    xmlNewChild(sig_node, nullptr, X"SignatureValue", X sig_b64.c_str());

    xmlNewChild(
            xmlNewChild(
                xmlNewChild(
                        sig_node,
                        nullptr,
                        X"KeyInfo",
                        nullptr),
                nullptr,
                X"X509Data",
                nullptr),
            nullptr,
            X"X509Certificate",
            X cert_b64.c_str());
}