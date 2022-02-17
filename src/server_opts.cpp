#include "server_opts.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "env.hpp"
#include "popl.hpp"

namespace fs = std::filesystem;

/**
 * Given a json configuration file, it will create and populate a ServerOptions struct.
 * The format of the JSON file is the following:
 * @code
 * {
 * 	"port": int,
 * 	"connections": int,
 * 	"timeout": int,
 * 	"ipv6": bool,
 * 	"ipv4": bool,
 * 	"certificate": string,
 * 	"privateKey": string
 * }
 * @endcode
 * @param file Path to the json config file to process.
 * @return A populated ServerOptions struct
 */
ServerOptions parse_options_from_file(const fs::path& file)
{
    std::ifstream i(file);
	nlohmann::json j;
	i >> j;

	auto get_or_default = [&j](const char* name, auto default_val) -> decltype(default_val)
	{
		if (j[name].is_null())
			return default_val;

		return j[name].get<decltype(default_val)>();
	};

	return {
		get_or_default("port", (uint16_t)8080),
		get_or_default("connections", 0),
		get_or_default("timeout", 180),
		get_or_default("ipv6", false),
		get_or_default("ipv4", true),
		get_or_default("certificate", std::string{}),
		get_or_default("privateKey", std::string{})
	};
}

ServerOptions parse_options(int argc, const char** argv)
{
	ServerOptions options;
	auto configFile = env::get_string("UTOPIA_CONFIG_FILE", "config.json");
	if (configFile.empty())
		configFile = "config.json";

	if (fs::exists(configFile))
		options = parse_options_from_file(configFile);

	// TODO(Jordan): Utopia is a temporary name, so I need to decide an actual
	//               name for this project.
	options.port = static_cast<uint16_t>(env::get_int("UTOPIA_PORT", options.port));
	options.max_connections = env::get_int("UTOPIA_MAX_CONNECTIONS", options.max_connections);
	options.timeout = env::get_int("UTOPIA_TIMEOUT", options.timeout);
	options.use_ipv4 = env::get_bool("UTOPIA_USE_IPV4", options.use_ipv4);
	options.use_ipv6 = env::get_bool("UTOPIA_USE_IPV6", options.use_ipv6);
	options.certificate = env::get_string("UTOPIA_CERTIFICATE", options.certificate);
	options.privateKey = env::get_string("UTOPIA_PRIVATE_KEY", options.privateKey);

	popl::OptionParser op("OPTIONS");
	auto help_opt = op.add<popl::Switch>("h", "help", "show this message");
	auto conf_opt = op.add<popl::Value<std::string>>("i", "config", "config file to use");
	auto port_opt = op.add<popl::Value<uint16_t>>("p", "port", "port to start the server on");
	auto conn_opt = op.add<popl::Value<int>>("c", "connections", "maximum connections to allow");
	auto time_opt = op.add<popl::Value<int>>("t", "timeout", "seconds of inactivity before connection is timed out");
	auto ipv4_opt = op.add<popl::Switch>("4", "use-ipv4", "allow IPv4 connections");
	auto ipv6_opt = op.add<popl::Switch>("6", "use-ipv6", "allow IPv6 connections");
	auto noipv4_opt = op.add<popl::Switch>("", "no-ipv4", "disallow IPv4 connections");
	auto noipv6_opt = op.add<popl::Switch>("", "no-ipv6", "disallow IPv6 connections");
	auto cert_opt = op.add<popl::Value<std::string>>("C", "cert", "certificate to authenticate with");
	auto key_opt = op.add<popl::Value<std::string>>("K", "key", "private key for the certificate");
	op.parse(argc, argv);

	if (help_opt->is_set())
	{
		std::cout << "usage:\n";
		std::cout << "\t" << argv[0] << " [OPTIONS]\n\n";
		std::cout << op << "\n";
		std::exit(EXIT_SUCCESS);
	}

	if (conf_opt->is_set() && fs::exists(conf_opt->value()))
		options = parse_options_from_file(conf_opt->value());

	if ((ipv4_opt->is_set() && noipv4_opt->is_set()) || (ipv6_opt->is_set() && noipv6_opt->is_set()))
		throw std::invalid_argument("ipv4/6 is both set and unset!");

	if (cert_opt->is_set() != key_opt->is_set())
	{
		throw std::invalid_argument("--cert and --key must be both set or unset!");
	}
	else if (cert_opt->is_set())
	{
		options.certificate = cert_opt->value();
		options.privateKey = key_opt->value();
	}

	if (port_opt->is_set())
		options.port = port_opt->value();
	if (conn_opt->is_set())
		options.max_connections = conn_opt->value();
	if (time_opt->is_set())
		options.timeout = time_opt->value();

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

	if (options.certificate.empty() || options.privateKey.empty())
		throw std::invalid_argument("A certificate and private key must be given for authentication!");

	return options;
}