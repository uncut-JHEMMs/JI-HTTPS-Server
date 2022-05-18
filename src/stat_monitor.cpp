#include "stat_monitor.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>

void monitor_stats(std::shared_ptr<Statistics> data)
{
    auto logger = spdlog::daily_logger_st("stat_logger", "logs/stats.log");
    logger->set_level(spdlog::level::trace);

    while (!data->should_close)
    {
        using namespace std::literals::chrono_literals;

        std::this_thread::sleep_for(1s);
        if (data->should_close)
            break;

        logger->info(
                "Requested bytes per sec: {}, Responded bytes per sec: {}",
                data->requestsPerSecond,
                data->responsePerSecond);

        data->clear();
    }
}

std::pair<std::shared_ptr<Statistics>, std::thread> stat_monitor::initialize()
{
    auto data = std::make_shared<Statistics>();
    std::thread monitor_thread{ monitor_stats, data };
    return { std::move(data), std::move(monitor_thread) };
}