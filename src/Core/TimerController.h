//
// Created by gnilk on 15.10.23.
//

#ifndef GOATEDIT_TIMERCONTROLLER_H
#define GOATEDIT_TIMERCONTROLLER_H

#include <thread>
#include <vector>
#include <mutex>
#include <optional>

#include "Timer.h"



namespace gedit {

    class TimerController {
    private:
    public:
        TimerController() = default;
        virtual ~TimerController();
        bool Start();
        void Stop();

        Timer::Ref CreateAndScheduleTimer(const Timer::DurationMS &durationMS, const Timer::TimerDelegate &onElapsed);

        bool ScheduleTimer(Timer::Ref &timer);
        bool RestartTimer(Timer::Ref &timer);


    protected:
        bool IsRunning();
        void TimerUpdateThreadLoop();
        size_t CollectTimersToExecute(std::vector<Timer::Ref> &outTimersToExecute);
        void RemoveAndExecuteTimers(std::vector<Timer::Ref> &timersToExecute);
        bool HaveTimer_NoLock(Timer::Ref &timer);
    protected:
        bool ScheduleTimer_NoLock(Timer::Ref &timer);

    private:
        bool isRunning = false;
        bool bQuit = false;
        std::thread runner;
        std::mutex timerLock;
        std::vector<Timer::Ref> activeTimers;
    };

}


#endif //GOATEDIT_TIMERCONTROLLER_H
