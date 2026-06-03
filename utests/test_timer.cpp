//
// Created by gnilk on 15.10.23.
//
#include <testinterface.h>
#include "Core/Timer.h"
#include "Core/RuntimeConfig.h"
#include <chrono>
#include <thread>
#include <atomic>


using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_timer(ITesting *t);
DLL_EXPORT int test_timer_exit(ITesting *t);
DLL_EXPORT int test_timer_create(ITesting *t);
DLL_EXPORT int test_timer_inrtc(ITesting *t);   // runtime config = rtc
}

// Poll HasExpired() up to 'budget'. Returns true if it expired in time, false on timeout.
// Bounded on purpose: a regressed/hung timer fails the test instead of blocking the suite forever.
static bool WaitForExpiry(const Timer::Ref &timer, std::chrono::milliseconds budget) {
    auto deadline = std::chrono::steady_clock::now() + budget;
    while (!timer->HasExpired()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

DLL_EXPORT int test_timer(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_timer_exit(ITesting *t) {
    return kTR_Pass;
}

// Single-shot expiry, handler invocation, and Restart() re-expiry - all with bounded waits.
DLL_EXPORT int test_timer_create(ITesting *t) {
    std::atomic<int> expiryCount = 0;
    auto timer = Timer::Create(100ms, [&expiryCount]() {
        expiryCount++;
    });

    // First expiry
    TR_ASSERT(t, WaitForExpiry(timer, 3000ms));
    TR_ASSERT(t, timer->HasExpired());
    TR_ASSERT(t, expiryCount == 1);

    // Restart clears the expiry flag and fires again
    timer->Restart();
    TR_ASSERT(t, !timer->HasExpired());
    TR_ASSERT(t, WaitForExpiry(timer, 3000ms));
    TR_ASSERT(t, expiryCount == 2);

    return kTR_Pass;
}

// Restart(newDuration) changes the interval, then Stop()/destruction must return promptly.
// (Originally intended as a RuntimeConfig 'rtc' timer test; RuntimeConfig has no timer API, so this
// exercises the duration-changing restart and the Stop()-without-lost-wakeup path instead.)
DLL_EXPORT int test_timer_inrtc(ITesting *t) {
    std::atomic<int> expiryCount = 0;
    // Long initial duration so it cannot fire before we shorten it.
    auto timer = Timer::Create(2000ms, [&expiryCount]() {
        expiryCount++;
    });

    // Shorten to a quick interval; it should now fire on the new (short) duration within budget.
    timer->Restart(50ms);
    TR_ASSERT(t, WaitForExpiry(timer, 3000ms));
    TR_ASSERT(t, expiryCount == 1);

    // Stop() must not lose its wakeup - the destructor join below would otherwise hang.
    timer->Stop();


    return kTR_Pass;
}
