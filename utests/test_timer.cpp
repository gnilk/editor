//
// Created by gnilk on 15.10.23.
//
#include <testinterface.h>
#include "Core/Timer.h"
#include "Core/TimerController.h"
#include <chrono>
#include <thread>


using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_timer(ITesting *t);
DLL_EXPORT int test_timer_exit(ITesting *t);
DLL_EXPORT int test_timer_create(ITesting *t);
}

DLL_EXPORT int test_timer(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_timer_exit(ITesting *t) {
    TimerController::Instance().Stop();
    return kTR_Pass;
}

DLL_EXPORT int test_timer_create(ITesting *t) {
    auto timer = Timer::Create(1000ms, [](){
        printf("Expired\n");
    });
    while(!timer->HasExpired()) {
        std::this_thread::yield();
    }
    return kTR_Pass;
}
