#include "utopia.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <filesystem>
#include <csignal>

#include "server_opts.hpp"
#include "resources.hpp"
#include "perf_monitor.hpp"
#include "stat_monitor.hpp"

void initialize_logging()
{
    spdlog::set_pattern("[%D %r] [thread %t] [%^%n - %l%$] %v");

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/server.log", 0, 0);
    file_sink->set_level(spdlog::level::trace);

    spdlog::flush_on(spdlog::level::trace);
    spdlog::flush_every(std::chrono::seconds(2));
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("Utopia", spdlog::sinks_init_list({file_sink, console_sink})));
}

std::condition_variable signal_cv;

void signal_callback_handler(int signum)
{
    if (signum == SIGINT)
        signal_cv.notify_all();
}

int utopia::run(int argc, const char** argv)
{
    using httpserver::create_webserver;
    using httpserver::http::http_utils;

    auto opts = parse_options(argc, argv);
    auto [perf_data, perf_thread] = perf_monitor::initialize();
    auto [stat_data, stat_thread] = stat_monitor::initialize();

    auto builder = create_webserver(opts.port)
            .digest_auth()
            .file_upload_target(httpserver::FILE_UPLOAD_DISK_ONLY)
            .generate_random_filename_on_upload()
            .max_connections(opts.max_connections)
            .connection_timeout(opts.timeout)
            .log_access([](const auto& url) {
                spdlog::info("ACCESSING: {}", url);
            })
            .log_error([](const auto& err) {
                spdlog::error("ERROR: {}", err);
            });

    if (opts.certificate.has_value() && opts.private_key.has_value())
    {
        builder
            .use_ssl()
            .https_mem_key(*opts.private_key)
            .https_mem_cert(*opts.certificate);
    }

    if (opts.thread_per_connection)
        builder.start_method(http_utils::THREAD_PER_CONNECTION);
    else
        builder.start_method(http_utils::INTERNAL_SELECT).max_threads(opts.max_threads);

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

    // Catching Ctrl-C (SIGINT) so I can gracefully shut down, it's not
    // the prettiest way, but it works.
    signal(SIGINT, signal_callback_handler);

    // Setup logging for the server
    initialize_logging();

    // Check for LMDB database (TODO: Get directory from config)
    std::shared_ptr<lmdb::env> env = nullptr;
    if (std::filesystem::is_directory("transactions.mdb"))
    {
        env = std::make_shared<lmdb::env>(lmdb::env::create());
        env->set_max_dbs(5);
        env->open("transactions.mdb", MDB_RDONLY, 0);
    }
    else
    {
        spdlog::critical("Unable to load LMDB database at `transactions.mdb`!");
        return 1;
    }

    httpserver::webserver ws = builder;
    auto resource_list = resources::resources(perf_data, stat_data, env);
    for (auto& resource : resource_list)
    {
        ws.register_resource(resource->endpoint(), resource.get(), resource->family());
    }

    spdlog::info("Starting server on port {}...", opts.port);
    ws.start();

    std::mutex sig_mtx;
    std::unique_lock<std::mutex> lock(sig_mtx);
    signal_cv.wait(lock);

    spdlog::info("Graceful shutdown requested, shutting down...");

    if (ws.is_running())
        ws.sweet_kill();

    perf_data->should_close = true;
    stat_data->should_close = true;

    // Interrupt any threads that are waiting for a value from the queue
    perf_data->access_queue.interrupt();

    perf_thread.join();
    stat_thread.join();

    spdlog::debug("All threads done and webserver gracefully killed.");
    return 0;
}
