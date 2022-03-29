#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <tuple>
#include <thread>

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
    std::mutex queue_mtx;
    std::condition_variable queue_cv;
    std::queue<AccessData> access_queue;
};

struct shutdown_exception : public std::exception
{};

namespace perf_monitor
{
    std::pair<std::shared_ptr<PerfData>, std::thread> initialize();
}
