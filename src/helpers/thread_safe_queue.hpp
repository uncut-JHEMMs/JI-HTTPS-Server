#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct interrupted_exception : public std::exception
{};

template<class T>
class ThreadSafeQueue
{
    std::deque<T> queue;
    std::mutex m;
    std::condition_variable cond;
    std::atomic_bool interrupted = false;

public:
    ThreadSafeQueue() = default;

    inline bool empty() const noexcept { return queue.empty(); }

    void interrupt()
    {
        interrupted = true;
        cond.notify_all();
    }

    void push(T&& val) noexcept
    {
        std::lock_guard<std::mutex> lock(m);
        queue.push_back(val);
        cond.notify_one();
    }

    void push(const T& val) noexcept
    {
        std::lock_guard<std::mutex> lock(m);
        queue.push_back(val);
        cond.notify_one();
    }

    T front()
    {
        std::unique_lock<std::mutex> lock(m);
        while (queue.empty())
        {
            cond.wait(lock);
            if (interrupted)
            {
                interrupted = false;
                throw interrupted_exception();
            }
        }

        return queue.front();
    }

    void pop()
    {
        std::unique_lock<std::mutex> lock(m);
        while (queue.empty())
        {
            cond.wait(lock);
            if (interrupted)
            {
                interrupted = false;
                throw interrupted_exception();
            }
        }
        queue.pop_front();
    }

    bool try_pop()
    {
        if (m.try_lock() && !queue.empty())
        {
            queue.pop_front();
            m.unlock();
            return true;
        }
        return false;
    }

    bool try_pop(T& val)
    {
        if (m.try_lock() && !queue.empty())
        {
            val = queue.front();
            queue.pop_front();
            m.unlock();
            return true;
        }

        return false;
    }

    T wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(m);
        while (queue.empty())
        {
            cond.wait(lock);
            if (interrupted)
            {
                interrupted = false;
                throw interrupted_exception();
            }
        }

        T val = queue.front();
        queue.pop_front();
        return val;
    }
};
