#define HAVE_GNUTLS
#include <httpserver.hpp>
#include <string>
#include <iostream>

#include "server_opts.hpp"

using namespace httpserver;

template<class T>
using Ref = std::shared_ptr<T>;

// TODO(Jordan): Replace this example stub with actual resources.
class hello_world_resource : public http_resource
{
public:
    const Ref<http_response> render(const http_request&)
    {
        return Ref<http_response>(new string_response("Hello, world!"));
    }
};

int main(int argc, const char** argv)
{
    auto opts = parse_options(argc, argv);

    auto builder = create_webserver()
        .port(opts.port)
        .use_ssl()
        .https_mem_key(opts.privateKey)
        .https_mem_cert(opts.certificate)
        .cred_type(http::http_utils::CERTIFICATE)
        .max_connections(opts.max_connections)
        .connection_timeout(opts.timeout)
        .log_access([](const auto& url) {
                std::cout << "ACCESSING: " << url << std::endl;
        })
        .log_error([](const auto& err) {
                std::cout << "ERROR: " << err << std::endl;
        });

    /**
     * There is no option to turn off IPv4, likely as a safety measure
     * to stop you from starting a server with no means of connecting
     * to it. So I have to manually check for the case of IPv6 and no
     * IPv4, or IPv6 and IPv4.
     *
     * The default behavior of the library is to just use IPv4.
     */
    if (opts.use_ipv6 && !opts.use_ipv4)
        builder.use_ipv6();
    else if (opts.use_ipv6 && opts.use_ipv4)
        builder.use_dual_stack();

    webserver ws = builder;
    hello_world_resource hwr;
    ws.register_resource("/helloworld", &hwr);
    ws.start(true);

    return 0;
}
