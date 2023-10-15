//
// Created by gnilk on 15.10.23.
//

#include <mutex>
#include <vector>
#include <thread>
#include "TimerController.h"

using namespace gedit;

// Not sure if I the timer should be 'self-managed' or if I should put it in the Editor (or RuntimeConfig class)
// If self-managed - I need to stop it when leaving the editor...
TimerController &TimerController::Instance() {
    static TimerController glbController;
    if (!glbController.IsRunning()) {
        glbController.StartController();
    }

    return glbController;
}

bool TimerController::IsRunning() {
    return isRunning;
}

bool TimerController::StartController() {
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

void TimerController::TimerUpdateThreadLoop() {
    while(!bQuit) {
        // Yield!!!
        std::this_thread::yield();

        // Block is needed for the lock_guard (released when goes out of scope)
        {
            std::lock_guard<std::mutex> guard(timerLock);

            std::vector<TimerInstance> timersToExecute;

            // Collect the timers we should execute
            auto nToExecute = CollectTimersToExecute(timersToExecute);

            if (nToExecute > 0) {
                // Remove and execute..
                // We remove first, so that timers can reschedule themselves during execution if they like to
                RemoveAndExecuteTimers(timersToExecute);
            }
        }
    }
}

//
// Go through and check if the time has elapsed for all timers, if they have - push them on the execution queue
// DO NOT remove them from active timer - currently we don't support recurrency timers but we will...
//
size_t TimerController::CollectTimersToExecute(std::vector<TimerInstance> &outTimersToExecute) {
    auto tNow = std::chrono::high_resolution_clock::now();
    // Figure out which timers to execute
    for(auto &t : activeTimers) {
        auto tDuration = tNow - t.tStart;
        if (tDuration > t.timer->GetDuration()) {
            outTimersToExecute.push_back(t);
        }
    }
    return outTimersToExecute.size();
}

void TimerController::RemoveAndExecuteTimers(std::vector<TimerInstance> &timersToExecute) {
    // Remove and Execute - we remove first, so the timers can reschedule themselve upon execution
    while(!timersToExecute.empty()) {
        auto t = timersToExecute.back();
        timersToExecute.pop_back();
        // Remove timer from active timers, this allows rescheduling during invoke...
        activeTimers.erase(std::remove_if(activeTimers.begin(),
                                          activeTimers.end(),
                                          [&](const TimerInstance &other) {
                                              return (other.timer.get() == t.timer.get());
                                          })
        );
        // Now invoke
        // Note: WE DO NOT push this on the editor main-thread, for now that's up to the creator of the timer
        t.timer->Invoke();
    }
}

//
// Schedule a timer, put it on the execution queue
//
bool TimerController::ScheduleTimer(Timer::Ref timer) {
    std::lock_guard<std::mutex> guard(timerLock);
    TimerInstance timerInstance;
    timerInstance.tStart = std::chrono::high_resolution_clock::now();
    timerInstance.timer = std::move(timer);
    activeTimers.push_back(timerInstance);
    return true;
}

//
// Check if a timer has expired
// Note: Take a raw pointer, we will be calling this from within the Timer instance with 'this'...
//
bool TimerController::HasExpired(Timer *ptrTimer) {
    std::lock_guard<std::mutex> guard(timerLock);
    for(auto &t : activeTimers) {
        if (t.timer.get() == ptrTimer) {
            return false;
        }
    }
    return true;
}
