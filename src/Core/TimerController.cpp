//
// Created by gnilk on 15.10.23.
//

#include <mutex>
#include <vector>
#include <thread>
#include "TimerController.h"

using namespace gedit;

TimerController::~TimerController() {
    if (isRunning) {
        Stop();
    }
}

bool TimerController::IsRunning() {
    return isRunning;
}

bool TimerController::Start() {
    if (isRunning) {
        return true;
    }

    isRunning = true;
    auto mThread = std::thread([this]() {
        TimerUpdateThreadLoop();
    });

    runner = std::move(mThread);

    return true;
}

// STOP - this must be called from the 'atexit' function
void TimerController::Stop() {
    bQuit = true;
    runner.join();
}

Timer::Ref TimerController::CreateAndScheduleTimer(const Timer::DurationMS &durationMS, const Timer::TimerDelegate &onElapsed) {
    std::lock_guard<std::mutex> guard(timerLock);
    auto timer = Timer::Create(durationMS, onElapsed);
    if (ScheduleTimer_NoLock(timer)) {
        return timer;
    }
    return nullptr;
}

bool TimerController::ScheduleTimer(Timer::Ref &timer) {
    std::lock_guard<std::mutex> guard(timerLock);
    return ScheduleTimer_NoLock(timer);
}

bool TimerController::RestartTimer(Timer::Ref &timer) {
    std::lock_guard<std::mutex> guard(timerLock);
    return ScheduleTimer_NoLock(timer);
}


void TimerController::TimerUpdateThreadLoop() {
    std::vector<Timer::Ref> timersToExecute;

    while(!bQuit) {
        // Yield!!!
        std::this_thread::yield();
        timersToExecute.clear();

        {
            std::lock_guard<std::mutex> guard(timerLock);
            CollectTimersToExecute(timersToExecute);
        }
        // DO NOT lock during this one - we lock per-execution here - as we remove timers
        RemoveAndExecuteTimers(timersToExecute);
    }
}

//
// Go through and check if the time has elapsed for all timers, if they have - push them on the execution queue
// DO NOT remove them from active timer - currently we don't support recurrency timers but we will...
//
size_t TimerController::CollectTimersToExecute(std::vector<Timer::Ref> &outTimersToExecute) {
    auto tNow = std::chrono::high_resolution_clock::now();
    // Figure out which timers to execute
    for(auto &t : activeTimers) {
        auto tDuration = tNow - t->tStart;
        if (tDuration > t->GetDuration()) {
            outTimersToExecute.push_back(t);
        }
    }
    return outTimersToExecute.size();
}

void TimerController::RemoveAndExecuteTimers(std::vector<Timer::Ref> &timersToExecute) {
    // Remove and Execute - we remove first, so the timers can reschedule themselve upon execution
    while(!timersToExecute.empty()) {
        auto t = timersToExecute.back();
        timersToExecute.pop_back();
        // Remove timer from active timers, this allows rescheduling during invoke...
        {
            std::lock_guard<std::mutex> guard(timerLock);
            activeTimers.erase(std::remove_if(activeTimers.begin(),
                                              activeTimers.end(),
                                              [&](const Timer::Ref &other) {
                                                  return (other == t);
                                              })
            );
        }
        // Now invoke
        // Note: WE DO NOT push this on the editor main-thread, for now that's up to the creator of the timer
        t->Invoke();
    }
}

//
// Internal no-lock versions, these are supposed to be called from within a locked mutext context
//

bool TimerController::ScheduleTimer_NoLock(Timer::Ref &timer) {
    timer->tStart = std::chrono::high_resolution_clock::now();
    // If this timer is not active - let's push it on the active queue...
    if (!HaveTimer_NoLock(timer)) {
        timer->isExpired = false;
        activeTimers.push_back(timer);
    }
    return true;
}

bool TimerController::HaveTimer_NoLock(Timer::Ref &timer) {
    for(auto &t : activeTimers) {
        if (t == timer) {
            return true;
        }
    }
    return false;

}
