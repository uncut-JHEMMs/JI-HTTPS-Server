#include <httpserver.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <utility>
#include "env.hpp"
#include "popl.hpp"

namespace fs = std::filesystem;
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

/**
 * A structure containing all of the configurable options of the server.
 *
 * There are multiple ways to set these options, which are:
 *   A config.json file
 *   Environment variables
 *   Command line flags
 *
 * Note that command line arguments override environment variables
 * Environment variables override config files
 * and config files override the defaults.
 *
 * So defaults -> config files -> environment variables -> command line flags.
 */
struct ServerOptions
{
    // TODO(Jordan): Maybe I should use more specific numeric types for these values?
    int port;
    int max_connections;
    int timeout;
    bool use_ipv6;
    bool use_ipv4;

    ServerOptions(int port = 8080, int max_connections = -1, int timeout = 180, bool use_ipv6 = false, bool use_ipv4 = true) :
        port(port),
        max_connections(max_connections),
        timeout(timeout),
        use_ipv6(use_ipv6),
        use_ipv4(use_ipv4)
    {}
};

/**
 * This will set options from a json formatted config file.
 * The syntax of which is like so:
 * {
 *   "port": int,
 *   "connections": int,
 *   "timeout": int,
 *   "ipv6": bool,
 *   "ipv4": bool
 * }
 */
ServerOptions parse_options_from_file(const fs::path& file)
{
    std::ifstream i(file);
    nlohmann::json j;
    i >> j;

    return { j["port"].get<int>(), j["connections"].get<int>(), j["timeout"].get<int>(), j["ipv6"].get<bool>(), j["ipv4"].get<bool>() };
}

ServerOptions parse_options(int argc, char** argv)
{
    ServerOptions options;
    auto configFile = env::get_string("UTOPIA_CONFIG_FILE", "config.json");
    if (configFile.empty())
        configFile = "config.json";

    if (fs::exists(configFile))
        options = parse_options_from_file(configFile);

    // TODO(Jordan): Utopia is a temporary name, so I need to decide an actual
    //               name for this project.
    options.port = env::get_int("UTOPIA_PORT", options.port);
    options.max_connections = env::get_int("UTOPIA_MAX_CONNECTIONS", options.max_connections);
    options.timeout = env::get_int("UTOPIA_TIMEOUT", options.timeout);
    options.use_ipv4 = env::get_bool("UTOPIA_USE_IPV4", options.use_ipv4);
    options.use_ipv6 = env::get_bool("UTOPIA_USE_IPV6", options.use_ipv6);

    popl::OptionParser op("OPTIONS");
    auto help_opt = op.add<popl::Switch>("h", "help", "show this message");
    auto port_opt = op.add<popl::Value<int>>("p", "port", "port to start the server on");
    auto conn_opt = op.add<popl::Value<int>>("c", "connections", "maximum connections to allow");
    auto time_opt = op.add<popl::Value<int>>("t", "timeout", "seconds of inactivity before connection is timed out");
    auto ipv4_opt = op.add<popl::Switch>("4", "use-ipv4", "allow IPv4 connections");
    auto ipv6_opt = op.add<popl::Switch>("6", "use-ipv6", "allow IPv6 connections");
    auto noipv4_opt = op.add<popl::Switch>("", "no-ipv4", "disallow IPv4 connections");
    auto noipv6_opt = op.add<popl::Switch>("", "no-ipv6", "disallow IPv6 connections");
    op.parse(argc, argv);

    if (help_opt->is_set())
    {
        std::cout << "usage:\n";
        std::cout << "\t" << argv[0] << " [OPTIONS]\n\n";
        std::cout << op << "\n";
        std::exit(EXIT_SUCCESS);
    }

    if ((ipv4_opt->is_set() && noipv4_opt->is_set()) || (ipv6_opt->is_set() && noipv6_opt->is_set()))
        throw std::invalid_argument("ipv4/6 is both set and unset!");

    if (port_opt->is_set())
        options.port = port_opt->value();
    if (conn_opt->is_set())
        options.max_connections = conn_opt->value();

    if (noipv4_opt->is_set())
        options.use_ipv4 = false;
    else if (ipv4_opt->is_set())
        options.use_ipv4 = true;

    if (noipv6_opt->is_set())
        options.use_ipv6 = false;
    else if (ipv6_opt->is_set())
        options.use_ipv6 = true;

    // TODO(Jordan): I wonder if I should make my own exception for this...
    //               invalid_argument works for what I'm trying to express.
    //               So I suppose it's fine?
    if (!options.use_ipv4 && !options.use_ipv6)
        throw std::invalid_argument("both ipv4 and ipv6 are disallowed, so no connections can be made!");

    return options;
}

int main(int argc, char** argv)
{
    auto opts = parse_options(argc, argv);

    auto builder = create_webserver()
        .port(opts.port)
        .max_connections(opts.max_connections)
        .connection_timeout(opts.timeout);

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
