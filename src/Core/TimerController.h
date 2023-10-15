//
// Created by gnilk on 15.10.23.
//

#ifndef GOATEDIT_TIMERCONTROLLER_H
#define GOATEDIT_TIMERCONTROLLER_H

#include <thread>
#include <vector>
#include <mutex>

#include "Timer.h"


namespace gedit {
    // This is the hidden timer management class
    class TimerController {
    private:
        struct TimerInstance {
            std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
            Timer::Ref timer;
        };
    public:
        virtual ~TimerController() = default;
        static TimerController &Instance();
        void Stop();
        bool ScheduleTimer(Timer::Ref timer);
        bool HasExpired(Timer *ptrTimer);


    protected:
        bool IsRunning();
        bool StartController();
        void TimerUpdateThreadLoop();
        size_t CollectTimersToExecute(std::vector<TimerInstance> &outTimersToExecute);
        void RemoveAndExecuteTimers(std::vector<TimerInstance> &timersToExecute);
    private:
        TimerController() = default;
    private:
        bool isRunning = false;
        bool bQuit = false;
        std::thread runner;
        std::mutex timerLock;
        std::vector<TimerInstance> activeTimers;
    };

}


#endif //GOATEDIT_TIMERCONTROLLER_H
