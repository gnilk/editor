//
// Created by gnilk on 09.02.23.
//

#ifndef EDITOR_SAFEQUEUE_H
#define EDITOR_SAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T>
class SafeQueue
{
public:
    using DurationMS = std::chrono::duration<uint64_t, std::ratio<1,1000> >;
public:
    SafeQueue(void)
            : q()
            , m()
            , c()
    {}

    ~SafeQueue(void)
    {}

    // Add an element to the queue.
    void push(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    bool empty() const {
        std::unique_lock<std::mutex> lock(m);
        return q.empty();
    }

    bool wait(uint64_t durationMs) {
        std::unique_lock<std::mutex> lock(m);
        if (c.wait_for(lock, DurationMS(durationMs)) == std::cv_status::timeout) {
            return false;
        }
        return true;
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T pop(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while(q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};


#endif //EDITOR_SAFEQUEUE_H
