//
// Created by gnilk on 15.10.23.
//

#include "Timer.h"

using namespace gedit;

Timer::~Timer() {
    Stop();
    timerThread.join();
}

Timer::Ref Timer::Create(const DurationMS &msToExpire, Timer::TimerDelegate onElapsed) {
    auto timer = std::make_shared<Timer>();
    timer->msDuration = msToExpire;
    timer->elapsedHandler = std::move(onElapsed);

    timer->Start();

    return timer;
}


void Timer::Stop() {
    wakeupReason = kReason::kStop;
    mycond.notify_one();
}

void Timer::Restart() {
    hasExpired = false;
    wakeupReason = kReason::kRestart;
    mycond.notify_one();
}

void Timer::Restart(const DurationMS &newDurationMS) {
    hasExpired = false;
    msDuration = newDurationMS;
    wakeupReason = kReason::kRestart;
    mycond.notify_one();
}

void Timer::Start() {
    // FIXME: Detect double start...
    auto mThread = std::thread([this]() {
        wakeupReason = kReason::kElapsed;
        Wait();
    });
    timerThread = std::move(mThread);
}


//
// IF creating threads would have been faster we could simply allow the thread to die and restart if needed...
// As it turns out - that's a really bad idea - so instead we have this slightly funky goto mess...
//
void Timer::Wait() {
    while(true) {
        std::unique_lock<std::mutex> lock(mymutex);
        restart:
        mycond.wait_for(lock, msDuration);
        // We can be notified for the following reasons...
        switch (wakeupReason) {
            case kReason::kRestart :
                wakeupReason = kReason::kElapsed;   // Assume this for the next round...
                goto restart;
            case kReason::kStop :
                goto leave;
            case kReason::kElapsed :
                break;
        }
        Invoke();

        // Let's NOT kill the thread...
        mycond.wait(lock);
        switch(wakeupReason) {
            case kReason::kRestart :
                wakeupReason = kReason::kElapsed;   // Assume this for the next round...
                goto restart;
            case kReason::kStop :
                goto leave;
        }
    }
    leave:
    hasExpired = true;
}

bool Timer::HasExpired() const {
    return hasExpired;
}

void Timer::Invoke() {
    hasExpired = true;
    if (elapsedHandler != nullptr) {
        elapsedHandler();
    }
}
