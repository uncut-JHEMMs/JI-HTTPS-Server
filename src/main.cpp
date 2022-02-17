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

#include "server_opts.hpp"

static std::atomic_int accesses{0};
static std::atomic_int errors{0};

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
        .https_mem_key(opts.privateKey)
        .https_mem_cert(opts.certificate)
        //.cred_type(http::http_utils::CERTIFICATE)
        .max_connections(opts.max_connections)
        .connection_timeout(opts.timeout)
        .log_access([](const auto& url) {
                accesses++;
                std::cout << "ACCESSING: " << url << std::endl;
        })
        .log_error([](const auto& err) {
                errors++;
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

    // Catching Ctrl-C (SIGINT) so I can gracefully shutdown, it's not
    // the prettiest way, but it works.
    signal(SIGINT, signal_callback_handler);

    webserver ws = builder;
    hello_world_resource hwr;
    ws.register_resource("/helloworld", &hwr);
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
