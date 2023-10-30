//
// Created by gnilk on 29.10.23.
//

#ifndef GOATEDIT_JOB_H
#define GOATEDIT_JOB_H

#include <memory>
#include <mutex>
#include <condition_variable>

namespace gedit {
    class Job {
    public:
        using Ref = std::shared_ptr<Job>;
    public:
        Job() = default;
        virtual ~Job() = default;
        void WaitComplete() {
            static std::mutex waitMutex;
            std::unique_lock lk(waitMutex);
            completionCond.wait(lk);
        }

        void NotifyComplete() {
            completionCond.notify_all();
        }

    private:
        std::condition_variable completionCond;
    };

}

#endif //GOATEDIT_JOB_H
