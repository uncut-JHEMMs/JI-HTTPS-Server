#include "utilities.hpp"

#include <vector>
#include <locale>
#include <fstream>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include "helpers/xml_builder.hpp"

std::string util::base64_encode(const uint8_t* buffer, size_t length)
{
    EVP_ENCODE_CTX* ctx = EVP_ENCODE_CTX_new();
    EVP_EncodeInit(ctx);

    std::string output;
    output.resize(EVP_ENCODE_LENGTH(length));

    int outlen;
    EVP_EncodeUpdate(ctx, (uint8_t*)output.data(), &outlen, buffer, (int)length);
    EVP_EncodeFinal(ctx, (uint8_t*)output.data(), &outlen);
    EVP_ENCODE_CTX_free(ctx);

    return output;
}

std::string util::read_file(const std::string_view& filename)
{
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(filename.data());
    stream.exceptions(std::ios_base::badbit);

    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size))
        out.append(buf, 0, static_cast<unsigned long>(stream.gcount()));
    out.append(buf, 0, static_cast<unsigned long>(stream.gcount()));
    return out;
}

std::shared_ptr<httpserver::string_response> util::make_xml_error(const std::string_view& msg, int code)
{
    using ::httpserver::string_response;

    XmlBuilder b;
    b
        .add_signature()
        .add_child("Data")
            .add_string("Error", msg);

    return std::make_shared<string_response>(b.serialize(), code, "application/xml");
}