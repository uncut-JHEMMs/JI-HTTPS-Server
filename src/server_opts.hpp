#pragma once

#include <string>

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
    int max_connections = 0;
    int timeout = 180;
    bool use_ipv6 = false;
    bool use_ipv4 = true;
    std::string certificate;
    std::string privateKey;
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
