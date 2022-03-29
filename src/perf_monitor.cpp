#include "perf_monitor.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>

void monitor_performance(std::shared_ptr<PerfData> data)
{
    auto logger = spdlog::daily_logger_st("perf_logger", "logs/perf.log");
    logger->set_level(spdlog::level::trace);

    std::unique_lock<std::mutex> lock(data->queue_mtx);
    while (!data->should_close)
    {
        data->queue_cv.wait(lock);

        if (data->should_close)
            break;

        while (!data->access_queue.empty())
        {
            auto elem = data->access_queue.back();
            data->access_queue.pop();

            switch (elem.status)
            {
                case AccessStatus::Success:
                    logger->info("{} successfully authenticated as {}!", elem.requester, elem.user);
                    break;
                case AccessStatus::NoUserProvided:
                    logger->warn("{} attempted to authenticate with no username!", elem.requester);
                    break;
                case AccessStatus::InvalidUserOrPassword:
                    logger->warn("{} failed to authenticate as {}!", elem.requester, elem.user);
                    break;
            }

            logger->flush();
        }
    }
}

std::pair<std::shared_ptr<PerfData>, std::thread> perf_monitor::initialize()
{
    auto data = std::make_shared<PerfData>();
    std::thread monitor_thread{ monitor_performance, data };
    return {std::move(data), std::move(monitor_thread)};
}
