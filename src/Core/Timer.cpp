//
// Created by gnilk on 15.10.23.
//

#include "Timer.h"

using namespace gedit;

Timer::~Timer() {
    Stop();
    if (timerThread.joinable()) {
        timerThread.join();
    }
}

Timer::Ref Timer::Create(const DurationMS &msToExpire, Timer::TimerDelegate onElapsed) {
    auto timer = std::make_shared<Timer>();
    timer->msDuration = msToExpire;
    timer->elapsedHandler = std::move(onElapsed);

    timer->Start();

    return timer;
}


void Timer::Stop() {
    {
        std::lock_guard<std::mutex> lock(mymutex);
        wakeupReason = kReason::kStop;
        commandPending = true;
    }
    mycond.notify_one();
}

void Timer::Restart() {
    {
        std::lock_guard<std::mutex> lock(mymutex);
        hasExpired = false;
        wakeupReason = kReason::kRestart;
        commandPending = true;
    }
    mycond.notify_one();
}

void Timer::Restart(const DurationMS &newDurationMS) {
    {
        std::lock_guard<std::mutex> lock(mymutex);
        hasExpired = false;
        msDuration = newDurationMS;
        wakeupReason = kReason::kRestart;
        commandPending = true;
    }
    mycond.notify_one();
}

void Timer::Start() {
    // FIXME: Detect double start...
    auto mThread = std::thread([this]() {
        Wait();
    });
    timerThread = std::move(mThread);
}

//
// Clears the pending command and returns its reason. MUST be called while holding 'mymutex'.
//
Timer::kReason Timer::ConsumeCommand() {
    auto reason = wakeupReason;
    commandPending = false;
    wakeupReason = kReason::kElapsed;    // assume 'let it elapse' for the next round
    return reason;
}

//
// The timer thread lives for the lifetime of the Timer (recreating threads per cycle is too slow).
// All shared state is guarded by 'mymutex', and every wait uses 'commandPending' as its predicate so
// a Stop()/Restart() that notifies just before we enter the wait is not lost.
//
void Timer::Wait() {
    std::unique_lock<std::mutex> lock(mymutex);
    while (true) {
        // Phase 1: count down 'msDuration', waking early if a command arrives.
        bool commanded = mycond.wait_for(lock, msDuration, [this]{ return commandPending; });
        if (commanded) {
            if (ConsumeCommand() == kReason::kStop) {
                break;
            }
            continue;   // kRestart: restart the countdown (with the possibly-updated msDuration)
        }

        // Phase 1 timed out with no command -> elapsed. Fire the handler WITHOUT the lock so it may
        // safely call back into Restart()/Stop() on this timer.
        lock.unlock();
        Invoke();
        lock.lock();

        // Phase 2: stay alive and idle until a command tells us to restart or stop.
        mycond.wait(lock, [this]{ return commandPending; });
        if (ConsumeCommand() == kReason::kStop) {
            break;
        }
        // kRestart: fall through and loop back into Phase 1
    }
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
