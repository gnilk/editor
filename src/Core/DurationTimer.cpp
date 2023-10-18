//
// Created by gnilk on 18.10.23.
//

#include "DurationTimer.h"

using namespace gedit;

DurationTimer::DurationTimer() {
    Reset();
}

void DurationTimer::Reset() {
    tStart = std::chrono::high_resolution_clock::now();
}

DurationTimer::DurationMS DurationTimer::Sample() {
    auto tNow = std::chrono::high_resolution_clock::now();
    auto duration = tNow - tStart;
    return std::chrono::duration_cast<DurationMS>(duration);
}
