#define HAVE_GNUTLS
#include <httpserver.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <ctime>
#include <signal.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "server_opts.hpp"

static std::atomic_int accesses{0};
static std::atomic_int errors{0};

using namespace httpserver;

template<class T>
using Ref = std::shared_ptr<T>;

#define MY_OPAQUE "11733b200778ce33060f31c9af70a870ba96ddd4"

class digest_test_resource : public http_resource
{
public:
    const Ref<http_response> render_GET(const http_request& req)
    {
        if (req.get_digested_user().empty())
        {
            return Ref<digest_auth_fail_response>(new digest_auth_fail_response("FAIL", "test@localhost", MY_OPAQUE, true));
        }
        else
        {
            bool reload_nonce = false;
            if (!req.check_digest_auth("test@localhost", "mypass", 300, &reload_nonce))
                return Ref<digest_auth_fail_response>(new digest_auth_fail_response("FAIL", "test@localhost", MY_OPAQUE, reload_nonce));
        }
        return Ref<http_response>(new string_response("SUCCESS", 200, "text/plain"));
    }
};

void monitor_performance(std::condition_variable& cv)
{
    std::ofstream out("perf.log", std::ios::ate);
    char buf[80];

    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);

    /**
     * This is some simple performance information, since
     * at the moment I don't have a full-fledge performance
     * monitoring suite.
     */
    while (cv.wait_for(lck, std::chrono::seconds(30)) == std::cv_status::timeout)
    {
        auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        strftime(buf, 80, "%c", localtime(&timenow));

        int taccess = accesses;
        int terror = errors;
        accesses = 0;
        errors = 0;
        out << "[perf | " << buf << "] There have been " << taccess << " access(es) and " << terror << " error(s) in the last 30 seconds." << std::endl;
    }

    out.close();
}

bool should_run = true;
void signal_callback_handler(int signum)
{
    if (signum == SIGINT)
        should_run = false;
}

int main(int argc, const char** argv)
{
    auto opts = parse_options(argc, argv);

    auto builder = create_webserver()
        .port(opts.port)
        .use_ssl()
        .https_mem_key(opts.private_key)
        .https_mem_cert(opts.certificate)
        .digest_auth()
        .max_connections(opts.max_connections)
        .connection_timeout(opts.timeout)
        .log_access([](const auto& url) {
                accesses++;
                spdlog::get("console")->info("ACCESSING: {}", url);
        })
        .log_error([](const auto& err) {
                errors++;
                spdlog::get("stderr")->error("ERROR: {}", err);
        });

    if (opts.thread_per_connection)
        builder.start_method(http::http_utils::THREAD_PER_CONNECTION);
    else
        builder.start_method(http::http_utils::INTERNAL_SELECT).max_threads(opts.max_threads);

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

    // Catching Ctrl-C (SIGINT) so I can gracefully shutdown, it's not
    // the prettiest way, but it works.
    signal(SIGINT, signal_callback_handler);

    // Creating some global logs
    spdlog::stdout_color_mt("console");
    spdlog::stderr_color_mt("stderr");

    webserver ws = builder;
    digest_test_resource hwr;
    ws.register_resource("/test_digest", &hwr);
    ws.start(false);

    std::condition_variable cv;
    std::thread perfthread{monitor_performance, std::ref(cv)};

    while (ws.is_running() && should_run)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (ws.is_running() && !should_run)
        ws.sweet_kill();

    cv.notify_one();
    perfthread.join();

    return 0;
}
