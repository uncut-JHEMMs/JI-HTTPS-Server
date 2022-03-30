#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <tuple>
#include <thread>

#include "thread_safe_queue.hpp"

enum AccessStatus
{
    NoUserProvided,
    InvalidUserOrPassword,
    Success
};

struct AccessData
{
    std::string requester;
    std::string user;
    AccessStatus status;
};

struct PerfData
{
    std::atomic_bool should_close;
    ThreadSafeQueue<AccessData> access_queue;
};

namespace perf_monitor
{
    std::pair<std::shared_ptr<PerfData>, std::thread> initialize();
}
