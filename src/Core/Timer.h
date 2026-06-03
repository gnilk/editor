//
// Created by gnilk on 15.10.23.
//

#ifndef GOATEDIT_TIMER_H
#define GOATEDIT_TIMER_H

#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace gedit {
    // This is the hidden timer management class
    class TimerController;

    class Timer {
        friend TimerController;
    public:
        using Ref = std::shared_ptr<Timer>;
        using TimerDelegate = std::function<void()>;
        using DurationMS = std::chrono::duration<uint64_t, std::ratio<1,1000> >;
    private:
        enum class kReason {
            kElapsed,
            kRestart,
            kStop,
        };
    public:
        Timer() = default;
        virtual ~Timer();


        static Ref Create(const DurationMS &msToExpire, TimerDelegate onElapsed);

        void Stop();
        void Restart();
        void Restart(const DurationMS &newDurationMS);

        const DurationMS &GetDuration() {
            return msDuration;
        }

        [[nodiscard]] bool HasExpired() const;


    protected:
        void Start();
        void Wait();    // Internal - waits for the condition to become available..
        void Invoke();
        kReason ConsumeCommand();   // Internal - call under 'mymutex'; clears the pending command and returns it.
    protected:
        std::mutex mymutex;
        std::condition_variable mycond;
        // wakeupReason/commandPending are guarded by 'mymutex'. commandPending is the wait predicate
        // that makes Stop()/Restart() immune to lost wakeups (a notify that races ahead of the wait).
        kReason wakeupReason = kReason::kElapsed;
        bool commandPending = false;
        DurationMS msDuration = {};
        std::atomic<bool> hasExpired = false;   // atomic so HasExpired() can be read lock-free from any thread
        TimerDelegate elapsedHandler = nullptr;
        std::thread timerThread;
        std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
    };

}


#endif //GOATEDIT_TIMER_H
