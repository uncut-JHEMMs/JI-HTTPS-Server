#include "utilities.hpp"

#include <string>
#include <vector>
#include <codecvt>
#include <locale>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <libxml/xmlwriter.h>
#include <xercesc/dom/DOM.hpp>

typedef std::vector<uint8_t> bytes;

constexpr const char XMLSignatureNS[] = "http://www.w3.org/2000/09/xmldsig#";
constexpr const char XMLCanonicalizationMethod[] = "http://www.w3.org/TR/2001/REC-xml-c14n-20010315#WithComments";
constexpr const char XMLSignatureMethod[] = "http://www.w3.org/2000/09/xmldsig#dsa-sha512";
constexpr const char XMLTransform[] = "http://www.w3.org/2000/09/xmldsig#enveloped-signature";
constexpr const char XMLDigestMethod[] = "http://www.w3.org/2000/09/xmldsig#sha512";

std::string base64_encode(const uint8_t* buffer, size_t length)
{
    EVP_ENCODE_CTX* ctx = EVP_ENCODE_CTX_new();
    EVP_EncodeInit(ctx);

    std::string output;
    output.resize(EVP_ENCODE_LENGTH(length));

    int outlen;
    EVP_EncodeUpdate(ctx, (uint8_t*)output.data(), &outlen, buffer, length);
    EVP_EncodeFinal(ctx, (uint8_t*)output.data(), &outlen);
    EVP_ENCODE_CTX_free(ctx);

    return output;
}

bytes compute_hash(size_t size, const uint8_t* data)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr);
    EVP_DigestUpdate(ctx, data, size);

    bytes hash;
    hash.resize(SHA512_DIGEST_LENGTH);
    uint hash_len;
    EVP_DigestFinal_ex(ctx, hash.data(), &hash_len);
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::string util::sign_document(xercesc::DOMDocument* doc)
{
    using namespace xercesc;

    auto serializer = doc->getImplementation()->createLSSerializer();
    XMLCh* data = serializer->writeToString(doc);
    size_t size = XMLString::stringLen(data) * sizeof(XMLCh);

    const auto* content = (const uint8_t*)data;

    size_t sig_len;
    bytes signature;
    bytes hash = compute_hash(size, content);

    FILE* fd = fopen("key.pem", "r");
    EVP_PKEY* key = PEM_read_PrivateKey(fd, nullptr, nullptr, nullptr);
    fclose(fd);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    EVP_DigestSignInit(ctx, nullptr, EVP_sha512(), nullptr, key);
    EVP_DigestSignUpdate(ctx, content, size);

    EVP_DigestSignFinal(ctx, nullptr, &sig_len);
    signature.resize(sig_len);
    EVP_DigestSignFinal(ctx, signature.data(), &sig_len);

    std::string hash_b64 = base64_encode(hash.data(), hash.size());
    std::string sig_b64 = base64_encode(signature.data(), sig_len);

    RSA* rsa_key = EVP_PKEY_get1_RSA(key);
    auto modulus = RSA_get0_n(rsa_key);
    auto modulus_size = BN_num_bytes(modulus);
    auto public_exponent = RSA_get0_e(rsa_key);
    auto public_exponent_size = BN_num_bytes(public_exponent);

    auto modulus_b64 = base64_encode((const uint8_t*)modulus, modulus_size);
    auto public_exponent_b64 = base64_encode((const uint8_t*)public_exponent, public_exponent_size);

    auto createElementWithAttribute = [&doc](const XMLCh* name, const XMLCh* attribute, const XMLCh* value) {
        auto elem = doc->createElement(name);
        elem->setAttribute(attribute, value);
        return elem;
    };

    auto createElementWithContent = [&doc](const XMLCh* name, const XMLCh* content) {
        auto elem = doc->createElement(name);
        elem->appendChild(doc->createTextNode(content));
        return elem;
    };

    auto config = doc->getDOMConfig();
    config->setParameter(X("canonical-form"), true);
    config->setParameter(X("comments"), false);
    doc->normalizeDocument();

    doc
        ->getDocumentElement()
        ->appendChild(createElementWithAttribute(X("Signature"), X("xmlns"), X(XMLSignatureNS)))
            ->appendChild(doc->createElement(X("SignedInfo")))
                ->appendChild(createElementWithAttribute(X("CanonicalizationMethod"), X("Algorithm"), X(XMLCanonicalizationMethod)))->getParentNode()
                ->appendChild(createElementWithAttribute(X("SignatureMethod"), X("Algorithm"), X(XMLSignatureMethod)))->getParentNode()
                ->appendChild(createElementWithAttribute(X("Reference"), X("URI"), X("")))
                    ->appendChild(doc->createElement(X("Transforms")))
                        ->appendChild(createElementWithAttribute(X("Transform"), X("Algorithm"), X(XMLTransform)))->getParentNode()
                        ->getParentNode()
                    ->appendChild(createElementWithAttribute(X("DigestMethod"), X("Algorithm"), X(XMLDigestMethod)))->getParentNode()
                    ->appendChild(createElementWithContent(X("DigestValue"), X(hash_b64.c_str())))->getParentNode()
                    ->getParentNode()
                ->getParentNode()
            ->appendChild(createElementWithContent(X("SignatureValue"), X(sig_b64.c_str())))->getParentNode()
            ->appendChild(doc->createElement(X("KeyInfo")))
                ->appendChild(doc->createElement(X("KeyValue")))
                    ->appendChild(doc->createElement(X("RSAKeyValue")))
                        ->appendChild(createElementWithContent(X("Modulus"), X(modulus_b64.c_str())))->getParentNode()
                        ->appendChild(createElementWithContent(X("Exponent"), X(public_exponent_b64.c_str())));

    XMLCh* new_data = serializer->writeToString(doc);
    size_t new_len = XMLString::stringLen(new_data);

    std::u16string str{ new_data, new_len};
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;

    return convert.to_bytes(str);
}