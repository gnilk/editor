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
            // Should unlock happen before notification???
            workMutex.unlock();
            completionCond.notify_all();
            isComplete = true;
        }

        void WaitComplete() {
            //
            // FIXME: This doesn't work - sometimes leads to race conditions
            //        where the job is complete but the condition is raced before we wait for it...
            //        which I find odd...
            //
            //std::unique_lock lk(workMutex);
            while(!isComplete) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                std::this_thread::yield();
                //completionCond.wait(lk);
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
