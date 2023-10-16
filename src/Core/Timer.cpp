//
// Created by gnilk on 15.10.23.
//

#include "Timer.h"
#include "TimerController.h"

using namespace gedit;

Timer::Ref Timer::Create(const DurationMS &msToExpire, Timer::TimerDelegate onElapsed) {
    auto timer = std::make_shared<Timer>();
    timer->msDuration = msToExpire;
    timer->elapsedHandler = std::move(onElapsed);

    return timer;
}

bool Timer::HasExpired() {
    return isExpired;
}

void Timer::Invoke() {
    isExpired = true;
    if (elapsedHandler != nullptr) {
        elapsedHandler();
    }
}
