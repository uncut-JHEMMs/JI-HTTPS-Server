#include "perf_monitor.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>

void monitor_performance(std::shared_ptr<PerfData> data)
{
    auto logger = spdlog::daily_logger_st("perf_logger", "logs/perf.log");
    logger->set_level(spdlog::level::trace);

    try
    {
        while (!data->should_close)
        {
            auto elem = data->access_queue.wait_and_pop();

            if (data->should_close)
                break;

            switch (elem.status)
            {
                case AccessStatus::Success:
                    logger->info(
                            "{} successfully authenticated as {}!",
                            elem.requester,
                            elem.user
                    );
                    break;
                case AccessStatus::NoUserProvided:
                    logger->warn(
                            "{} attempted to authenticate with no username!",
                            elem.requester
                    );
                    break;
                case AccessStatus::InvalidUserOrPassword:
                    logger->warn(
                            "{} failed to authenticate as {}!",
                            elem.requester,
                            elem.user
                    );
                    break;
            }

            logger->flush();
        }
    }
    catch (interrupted_exception&)
    {}
}

std::pair<std::shared_ptr<PerfData>, std::thread> perf_monitor::initialize()
{
    auto data = std::make_shared<PerfData>();
    std::thread monitor_thread{ monitor_performance, data };
    return {std::move(data), std::move(monitor_thread)};
}
