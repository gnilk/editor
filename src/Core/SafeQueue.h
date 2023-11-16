//
// Created by gnilk on 09.02.23.
//

#ifndef EDITOR_SAFEQUEUE_H
#define EDITOR_SAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

// Thread safe queue
template <class T>
class SafeQueue
{
public:
    using DurationMS = std::chrono::duration<uint64_t, std::ratio<1,1000> >;
public:
    SafeQueue() = default;
    // FIX-ME: should we do 'c.notify_one' in case someone is stuck
    virtual ~SafeQueue() {
        c.notify_one();
    }

    // Add an element to the queue.
    void push(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(t));
        c.notify_one();
    }

    bool is_empty() const {
        std::unique_lock<std::mutex> lock(m);
        return q.empty();
    }

    // Wait for an element - IF the queue is empty
    bool wait(uint64_t durationMs) {
        std::unique_lock<std::mutex> lock(m);
        if(q.empty()) {
            if (c.wait_for(lock, DurationMS(durationMs)) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    // Get the "front"-element.
    // If the queue is empty, wait until an element becomes available
    std::optional<T> pop(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while(q.empty())
        {
            // wait - forever...
            c.wait(lock);
        }
        // Stopped?
        if (q.empty()) {
            return {};
        }

        T val = std::move(q.front());
        q.pop();
        return val; // optional has move-ctor...
    }

private:
    std::queue<T> q = {};
    mutable std::mutex m = {};
    std::condition_variable c = {};
};


#endif //EDITOR_SAFEQUEUE_H
