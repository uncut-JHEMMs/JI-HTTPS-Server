#pragma once

#include <string>
#include <optional>

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
    uint16_t port = 8080;
    uint16_t max_connections = 0;
    uint16_t timeout = 180;
    uint16_t max_threads = 1;
    bool thread_per_connection = false;
    bool use_ipv6 = false;
    bool use_ipv4 = true;
    std::optional<std::string> certificate;
    std::optional<std::string> private_key;
};

/**
 * Generates a ServerOptions structure with values from config files,
 * environment variables, and command line arguments, and sanity
 * checks the options to make sure they're valid.
 *
 * @param argc Argument count passed in from `main`
 * @param argv Argument list passed in from `main`
 * @returns A populated and sanity checked ServerOptions struct
 * @throws std::invalid_argument Thrown whenever a sanity check fails
 */
ServerOptions parse_options(int argc, const char** argv);
