//
// Created by gnilk on 15.10.23.
//

#include <mutex>
#include <vector>
#include <thread>
#include "TimerController.h"

using namespace gedit;

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

void TimerController::Stop() {
    bQuit = true;
    runner.join();
}

void TimerController::TimerUpdateThreadLoop() {
    while(!bQuit) {
        // Yield!!!
        std::this_thread::yield();

        {
            std::vector<TimerInstance> timersToExecute;
            std::lock_guard<std::mutex> guard(timerLock);
            auto tNow = std::chrono::high_resolution_clock::now();
            // Figure out which timers to execute
            for(auto &t : activeTimers) {
                auto tDuration = tNow - t.tStart;
                if (tDuration > t.timer->GetDuration()) {
                    timersToExecute.push_back(t);
                }
            }

            // Execute them
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
                t.timer->Invoke();
            }
        }
    }
}

bool TimerController::ScheduleTimer(Timer::Ref timer) {
    std::lock_guard<std::mutex> guard(timerLock);
    TimerInstance timerInstance;
    timerInstance.tStart = std::chrono::high_resolution_clock::now();
    timerInstance.timer = std::move(timer);
    activeTimers.push_back(timerInstance);
    return true;
}

bool TimerController::HasExpired(Timer *ptrTimer) {
    std::lock_guard<std::mutex> guard(timerLock);
    for(auto &t : activeTimers) {
        if (t.timer.get() == ptrTimer) {
            return false;
        }
    }
    return true;
}
