//
// Created by gnilk on 29.10.23.
//

#ifndef GOATEDIT_JOB_H
#define GOATEDIT_JOB_H

#include <memory>
#include <mutex>
#include <condition_variable>

namespace gedit {
    // DO NOT REUSE!
    class Job {
    public:
        using Ref = std::shared_ptr<Job>;
    public:
        Job() = default;
        virtual ~Job() = default;
        void Begin() {
            workMutex.lock();
        }

        void NotifyComplete() {
            workMutex.unlock();
            completionCond.notify_all();
            isComplete = true;
        }

        void WaitComplete() {
            std::unique_lock lk(workMutex);
            while(!isComplete) {
                completionCond.wait(lk);
            }
            isComplete = false;
        }


    private:
        bool isComplete = false;
        std::mutex workMutex;
        std::condition_variable completionCond;
    };

}

#endif //GOATEDIT_JOB_H
