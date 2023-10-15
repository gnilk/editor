//
// Created by gnilk on 15.10.23.
//

#include "Timer.h"
#include "TimerController.h"

using namespace gedit;
using namespace std::chrono_literals;

Timer::Ref Timer::Create(const DurationMS &msToExpire, Timer::TimerDelegate onElapsed) {
    auto timer = std::make_shared<Timer>();
    timer->msDuration = msToExpire;
    timer->elapsedHandler = std::move(onElapsed);

    TimerController::Instance().ScheduleTimer(timer);

    return timer;
}

bool Timer::HasExpired() {
    return TimerController::Instance().HasExpired(this);
}

void Timer::Invoke() {
    elapsedHandler();
}




