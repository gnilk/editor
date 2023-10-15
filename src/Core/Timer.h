//
// Created by gnilk on 15.10.23.
//

#ifndef GOATEDIT_TIMER_H
#define GOATEDIT_TIMER_H

#include <memory>
#include <functional>
#include <chrono>

namespace gedit {
    class Timer {
    public:
        using Ref = std::shared_ptr<Timer>;
        using TimerDelegate = std::function<void()>;
        using DurationMS = std::chrono::duration<uint64_t, std::ratio<1,1000> >;
    public:
        Timer() = default;
        virtual ~Timer() = default;
        static Ref Create(const DurationMS &msToExpire, TimerDelegate onElapsed);

        const DurationMS &GetDuration() {
            return msDuration;
        };
        void Invoke();
        bool HasExpired();
    protected:
    private:

        DurationMS msDuration;
        TimerDelegate elapsedHandler;
    };


}


#endif //GOATEDIT_TIMER_H
