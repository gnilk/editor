//
// Created by gnilk on 15.10.23.
//

#ifndef GOATEDIT_TIMER_H
#define GOATEDIT_TIMER_H

#include <memory>
#include <functional>
#include <chrono>

namespace gedit {
    // This is the hidden timer management class
    class TimerController;

    class Timer {
        friend TimerController;
    public:
        using Ref = std::shared_ptr<Timer>;
        using TimerDelegate = std::function<void()>;
        using DurationMS = std::chrono::duration<uint64_t, std::ratio<1,1000> >;
    public:
        Timer() = default;
        virtual ~Timer() = default;


        void SetDuration(const DurationMS &newDurationMS) {
            msDuration = newDurationMS;
        }
        const DurationMS &GetDuration() {
            return msDuration;
        }

        bool HasExpired();


    protected:
        static Ref Create(const DurationMS &msToExpire, TimerDelegate onElapsed);

        void Invoke();
    protected:
        DurationMS msDuration = {};
        bool isExpired = false;
        TimerDelegate elapsedHandler = nullptr;
        std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
    };

}


#endif //GOATEDIT_TIMER_H
