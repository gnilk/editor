//
// Created by gnilk on 29.10.23.
//

#ifndef GOATEDIT_JOB_H
#define GOATEDIT_JOB_H

#include <memory>
#include <mutex>
#include <condition_variable>
#include <future>

namespace gedit {

    class Job {
    public:
        using Ref = std::shared_ptr<Job>;

    public:
        Job() {
            Reset();
        }

        void Begin() {
            Reset();
        }

        void NotifyComplete() {
            promise.set_value();
        }

        void WaitComplete() {
            future.wait();
        }

    private:
        void Reset() {
            promise = std::promise<void>{};
            future = promise.get_future();
        }

    private:
        std::promise<void> promise;
        std::future<void> future;
    };

}

#endif //GOATEDIT_JOB_H
