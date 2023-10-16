//
// Created by gnilk on 15.10.23.
//
#include <testinterface.h>
#include "Core/Timer.h"
#include "Core/TimerController.h"
#include "Core/RuntimeConfig.h"
#include <chrono>
#include <thread>


using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_timer(ITesting *t);
DLL_EXPORT int test_timer_exit(ITesting *t);
DLL_EXPORT int test_timer_create(ITesting *t);
DLL_EXPORT int test_timer_inrtc(ITesting *t);   // runtime config = rtc
}

DLL_EXPORT int test_timer(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_timer_exit(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_timer_create(ITesting *t) {
    TimerController timerController;
    timerController.Start();

    auto timer = timerController.CreateAndScheduleTimer(1000ms, [](){
        static int expCounter = 0;
        printf("Expired: %d\n", expCounter);
        expCounter++;
    });

    printf("Waiting for expiry\n");
    while(!timer->HasExpired()) {
        std::this_thread::yield();
    }
    std::this_thread::yield();

    printf("Restarting\n");
    timerController.RestartTimer(timer);

    printf("Waiting for expiry again\n");
    while(!timer->HasExpired()) {
        std::this_thread::yield();
    }
    return kTR_Pass;
}

DLL_EXPORT int test_timer_inrtc(ITesting *t) {
    auto &tc = RuntimeConfig::Instance().GetTimerController();


    auto timer = tc.CreateAndScheduleTimer(1000ms, [](){
        static int expCounter = 0;
        printf("Expired: %d\n", expCounter);
        expCounter++;
    });

    printf("Waiting for expiry\n");
    while(!timer->HasExpired()) {
        std::this_thread::yield();
    }
    std::this_thread::yield();

    printf("Restarting\n");
    tc.RestartTimer(timer);

    printf("Waiting for expiry again\n");
    while(!timer->HasExpired()) {
        std::this_thread::yield();
    }

    return kTR_Pass;
}
