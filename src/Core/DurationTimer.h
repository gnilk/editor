//
// Created by gnilk on 18.10.23.
//

#ifndef GOATEDIT_DURATIONTIMER_H
#define GOATEDIT_DURATIONTIMER_H


#include <chrono>

namespace gedit {
    class DurationTimer {
    public:
        using DurationTimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
        using DurationMS = std::chrono::milliseconds;
    public:
        DurationTimer();
        virtual ~DurationTimer() = default;

        void Reset();
        DurationMS Sample();
    private:
        DurationTimePoint tStart = {};
    };
}


#endif //GOATEDIT_DURATIONTIMER_H
