#pragma once

#include <atomic>
#include <tuple>
#include <memory>
#include <thread>

struct Statistics
{
    std::atomic_uint totalRequests;
    std::atomic_uint totalResponses;
    std::atomic_uint responsePerSecond;
    std::atomic_uint requestsPerSecond;
    std::atomic_bool should_close;

    inline void clear()
    {
        responsePerSecond = 0;
        requestsPerSecond = 0;
    }
};

namespace stat_monitor
{
    std::pair<std::shared_ptr<Statistics>, std::thread> initialize();
}